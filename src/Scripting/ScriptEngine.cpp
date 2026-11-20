#include "Scripting/ScriptEngine.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "ECS/Components/ScriptComponent.h"
#include "ECS/World.h"
#include "Scripting/Bindings/BehaviourBindings.h"
#include "Scripting/Bindings/EntityBindings.h"
#include "Scripting/Bindings/InputBindings.h"
#include "Scripting/Bindings/LogBindings.h"
#include "Scripting/Bindings/VectorBindings.h"
#include "Scripting/Bindings/WorldBindings.h"
#include "Scripting/ScriptExecutionContext.h"
#include "Scripting/LuaInstructionLimit.h"

#include <EASTL/optional.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sol/sol.hpp>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NeneEngine
{
	namespace
	{
		constexpr int kConsoleInstructionLimit = 100'000;
		constexpr int kScriptLoadInstructionLimit = 1'000'000;

#ifndef NDEBUG
		void StartLuaDebugger(sol::state& lua)
		{
			char enabled[2]{};
			std::size_t size = 0;
			if (getenv_s(&size, enabled, sizeof(enabled), "LOCAL_LUA_DEBUGGER_VSCODE") != 0 ||
			    std::string_view(enabled) != "1")
				return;

			lua.open_libraries(sol::lib::debug, sol::lib::io, sol::lib::os, sol::lib::coroutine);
			const auto result = lua.safe_script(R"lua(
				local path = assert(os.getenv("LOCAL_LUA_DEBUGGER_FILEPATH"), "Lua debugger path is missing")
				local debugger = assert(loadfile(path, "t"))()
				package.loaded["lldebugger"] = debugger
				debugger.start()
			)lua", sol::script_pass_on_error);
			if (!result.valid())
			{
				const sol::error error = result;
				NENE_LOG_WARN("Could not start the Lua debugger: {}", error.what());
			}
		}
#endif

		int TraceLuaError(lua_State* state)
		{
			const char* message = lua_tostring(state, 1);
			luaL_traceback(state, state, message != nullptr ? message : "Lua raised a non-string error", 1);
			return 1;
		}

		eastl::optional<std::filesystem::file_time_type> ReadLastWriteTime(const std::filesystem::path& path)
		{
			std::error_code error;
			const auto writeTime = std::filesystem::last_write_time(path, error);
			if (error) return eastl::nullopt;
			return writeTime;
		}

		bool IsValidScriptPath(const std::filesystem::path& path)
		{
			return !path.empty() && !path.has_root_path() && *path.begin() != "..";
		}

		eastl::optional<ScriptFieldValue> ReadFieldValue(const sol::object& value)
		{
			switch (value.get_type())
			{
			case sol::type::boolean:
				return value.as<bool>();
			case sol::type::string:
				return value.as<std::string>();
			case sol::type::number:
			{
				lua_State* luaState = value.lua_state();
				value.push();
				const bool isInteger = lua_isinteger(luaState, -1) != 0;
				lua_pop(luaState, 1);
				if (isInteger)
				{
					const auto number = value.as<lua_Integer>();
					if (std::in_range<int>(number)) return static_cast<int>(number);
					return eastl::nullopt;
				}

				const auto number = value.as<lua_Number>();
				if (std::isfinite(number) && std::abs(number) <= (std::numeric_limits<float>::max)())
					return static_cast<float>(number);
				return eastl::nullopt;
			}
			case sol::type::userdata:
				if (value.is<glm::vec3>()) return value.as<glm::vec3>();
				return eastl::nullopt;
			default:
				return eastl::nullopt;
			}
		}

		eastl::optional<ScriptFieldSchema> ReadFieldSchema(const sol::table& scriptClass, std::string_view fileName)
		{
			const auto fields = scriptClass.raw_get<sol::object>("fields");
			ScriptFieldSchema schema;
			if (fields == sol::nil)
			{
				if (scriptClass.raw_get<sol::object>("field_metadata") != sol::nil) return eastl::nullopt;
				return schema;
			}
			if (fields.get_type() != sol::type::table)
			{
				NENE_LOG_ERROR("Script '{}': fields must be a table", fileName);
				return eastl::nullopt;
			}

			for (const auto& [key, value] : fields.as<sol::table>())
			{
				if (key.get_type() != sol::type::string)
				{
					NENE_LOG_ERROR("Script '{}': field names must be strings", fileName);
					return eastl::nullopt;
				}

				auto name = key.as<std::string>();
				auto defaultValue = ReadFieldValue(value);
				if (!defaultValue || !IsValidScriptFieldValue({name, *defaultValue}, *defaultValue))
				{
					NENE_LOG_ERROR("Script '{}': invalid default value for field '{}'", fileName, name);
					return eastl::nullopt;
				}
				schema.push_back({std::move(name), std::move(*defaultValue)});
			}

			const auto metadata = scriptClass.raw_get<sol::object>("field_metadata");
			if (metadata != sol::nil)
			{
				if (metadata.get_type() != sol::type::table)
				{
					NENE_LOG_ERROR("Script '{}': field_metadata must be a table", fileName);
					return eastl::nullopt;
				}
				for (const auto& [key, value] : metadata.as<sol::table>())
				{
					if (key.get_type() != sol::type::string || value.get_type() != sol::type::table)
					{
						NENE_LOG_ERROR("Script '{}': invalid field metadata entry", fileName);
						return eastl::nullopt;
					}
					const auto name = key.as<std::string>();
					auto it = std::find_if(schema.begin(), schema.end(), [&](const auto& f) { return f.name == name; });
					if (it == schema.end())
					{
						NENE_LOG_ERROR("Script '{}': metadata refers to unknown field '{}'", fileName, name);
						return eastl::nullopt;
					}
					const sol::table spec = value.as<sol::table>();
					const auto type = spec.raw_get<sol::object>("type");
					const char* expected[] = {"bool", "int", "float", "string", "vec3"};
					if (type.get_type() != sol::type::string ||
					    (type.as<std::string>() != expected[it->defaultValue.index()] &&
					     !(type.as<std::string>() == "enum" && std::holds_alternative<std::string>(it->defaultValue))))
					{
						NENE_LOG_ERROR("Script '{}': invalid type for field '{}'", fileName, name);
						return eastl::nullopt;
					}
					const auto min = spec.raw_get<sol::object>("min");
					const auto max = spec.raw_get<sol::object>("max");
					if (min != sol::nil || max != sol::nil)
					{
						if (min == sol::nil || max == sol::nil || min.get_type() != sol::type::number ||
						    max.get_type() != sol::type::number)
							return eastl::nullopt;
						if (std::holds_alternative<int>(it->defaultValue))
						{
							auto low = ReadFieldValue(min), high = ReadFieldValue(max);
							if (!low || !high || !std::holds_alternative<int>(*low) || !std::holds_alternative<int>(*high) ||
							    std::get<int>(*low) >= std::get<int>(*high)) return eastl::nullopt;
							it->minimum = *low;
							it->maximum = *high;
						}
						else if (std::holds_alternative<float>(it->defaultValue))
						{
							const double low = min.as<double>(), high = max.as<double>();
							if (!std::isfinite(low) || !std::isfinite(high) || low >= high ||
							    std::abs(low) > (std::numeric_limits<float>::max)() ||
							    std::abs(high) > (std::numeric_limits<float>::max)() ||
							    static_cast<float>(low) >= static_cast<float>(high)) return eastl::nullopt;
							it->minimum = static_cast<float>(low);
							it->maximum = static_cast<float>(high);
						}
						else return eastl::nullopt;
					}
					const auto options = spec.raw_get<sol::object>("options");
					if (type.as<std::string>() == "enum")
					{
						if (options.get_type() != sol::type::table) return eastl::nullopt;
						const auto list = options.as<sol::table>();
						const auto count = list.size();
						if (count == 0) return eastl::nullopt;
						for (size_t i = 1; i <= count; ++i)
						{
							const auto option = list.raw_get<sol::object>(i);
							if (option.get_type() != sol::type::string) return eastl::nullopt;
							auto text = option.as<std::string>();
							if (std::find(it->options.begin(), it->options.end(), text) != it->options.end()) return eastl::nullopt;
							it->options.push_back(std::move(text));
						}
						size_t actual = 0;
						for (const auto& entry : list) { (void)entry; ++actual; }
						if (actual != count) return eastl::nullopt;
					}
					else if (options != sol::nil) return eastl::nullopt;
					if (!IsValidScriptFieldValue(*it, it->defaultValue)) return eastl::nullopt;
				}
			}

			std::ranges::sort(schema, {}, &ScriptFieldDefinition::name);
			return schema;
		}
	}

	struct ScriptEngine::Impl
	{
		struct LoadedScript
		{
			sol::environment environment;
			sol::table scriptClass;
			ScriptFieldSchema fieldSchema;
			bool usesRequire = false;
		};

		struct TrackedScript
		{
			eastl::optional<LoadedScript> loaded;
			eastl::optional<std::filesystem::file_time_type> lastAttemptedWriteTime;
			std::uint64_t revision = 0;
			bool loadFailed = false;
		};

		struct TrackedModule
		{
			std::filesystem::path path;
			eastl::optional<std::filesystem::file_time_type> lastAttemptedWriteTime;
		};

		ScriptExecutionContext execution;
		// Destroy loaded scripts and their Lua references before the interpreter
		sol::state lua;
		sol::protected_function executeConsoleCommand;
		std::weak_ptr<const ECS::WorldLifetime> worldLifetime;
		std::filesystem::path scriptsRoot;
		std::unordered_map<std::filesystem::path, TrackedScript> scripts;
		std::unordered_map<std::string, TrackedModule> modules;
		std::uint64_t scriptCacheGeneration = 0;
		bool moduleReloadPending = false;
		bool scriptUsesRequire = false;

		eastl::optional<LoadedScript> TryLoadScript(const std::filesystem::path& path);
		void ApplyScript(const std::filesystem::path& path, TrackedScript& script, LoadedScript loaded, bool isReload);
		void LoadTrackedScript(const std::filesystem::path& path, TrackedScript& script,
		                       eastl::optional<std::filesystem::file_time_type> writeTime, bool isReload);
		const sol::table* LoadScript(const std::filesystem::path& path);
		void ReloadScripts(bool force, const std::filesystem::path* target = nullptr);
		void ReloadModules(bool force, const std::filesystem::path* target);
	};

	ScriptEngine::ScriptEngine() = default;
	ScriptEngine::~ScriptEngine() = default;

	void ScriptEngine::Initialize(const IInputHandler& input, ECS::World& world, PrefabLibrary& prefabs)
	{
		if (m_impl) return;

		auto impl = std::make_unique<Impl>();
		impl->worldLifetime = world.GetLifetime();
		const std::filesystem::path relativeRoot{"assets/scripts"};
		impl->scriptsRoot = ResolveFromExecutionRoots(relativeRoot, true);
		if (impl->scriptsRoot.empty()) impl->scriptsRoot = GetExecutableDirectory() / relativeRoot;

		auto& lua = impl->lua;
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::package);
		sol::protected_function::set_default_handler(sol::object(lua.lua_state(), sol::in_place, &TraceLuaError));

		sol::table package = lua["package"];
		package["path"] = (impl->scriptsRoot / "?.lua").generic_string() + ";" +
		                  (impl->scriptsRoot / "?" / "init.lua").generic_string();
		// Keep require available for Lua modules and exclude native library loading
		package["loadlib"] = sol::nil;
		package["cpath"] = "";
		sol::table searchers = package["searchers"];
		searchers[3] = sol::nil;
		searchers[4] = sol::nil;

		const auto installModuleTracking = lua.safe_script(R"lua(
			local searchpath = package.searchpath
			local searcher = package.searchers[2]
			local requireModule = require
			return function(recordFile, recordRequire)
				package.searchers[2] = function(name)
					local path = searchpath(name, package.path)
					if path then recordFile(name, path) end
					return searcher(name)
				end
				require = function(name)
					recordRequire()
					return requireModule(name)
				end
			end
		)lua").get<sol::protected_function>();
		const auto moduleTracking = installModuleTracking(
		    [tracker = impl.get()](std::string name, std::string filename)
		    {
			    std::filesystem::path path(std::move(filename));
			    auto writeTime = ReadLastWriteTime(path);
			    tracker->modules.insert_or_assign(std::move(name), Impl::TrackedModule{std::move(path), writeTime});
		    },
		    [tracker = impl.get()] { tracker->scriptUsesRequire = true; });
		if (!moduleTracking.valid()) throw moduleTracking.get<sol::error>();

		auto nene = lua.create_named_table("nene");
		BindVectorApi(nene);
		BindBehaviourApi(nene);
		BindInputApi(nene, input);
		BindLogApi(nene);
		BindEntityApi(nene, impl->execution);
		BindWorldApi(nene, world, prefabs, impl->execution);

		const auto consoleCommand = lua.safe_script(R"lua(
			local environment = setmetatable({}, { __index = _G })

			local function format_values(...)
				local values = table.pack(...)
				for index = 1, values.n do
					values[index] = tostring(values[index])
				end
				return table.concat(values, "\t", 1, values.n)
			end

			environment.print = function(...)
				nene.log.info(format_values(...))
			end

			return function(command, selected_fields)
				rawset(environment, "selected_fields", selected_fields)
				local chunk = load("return " .. command, "=Console", "t", environment)
				if not chunk then
					local message
					chunk, message = load(command, "=Console", "t", environment)
					if not chunk then error(message, 0) end
				end

				local values = table.pack(chunk())
				if values.n > 0 then
					return format_values(table.unpack(values, 1, values.n))
				end
			end
		)lua");
		impl->executeConsoleCommand = consoleCommand.get<sol::protected_function>();

#ifndef NDEBUG
		StartLuaDebugger(lua);
#endif

		m_impl = std::move(impl);
		NENE_LOG_INFO("Script engine initialized");
	}

	void ScriptEngine::Shutdown()
	{
		if (!m_impl) return;
		if (const auto lifetime = m_impl->worldLifetime.lock(); lifetime && lifetime->world)
			lifetime->world->NotifyPlayStopping();
		m_impl.reset();
		NENE_LOG_INFO("Script engine shut down");
	}

	void ScriptEngine::ExecuteConsoleCommand(std::string_view command, const ECS::ScriptComponent* selectedScript)
	{
		if (command.find_first_not_of(" \t\r\n") == std::string_view::npos) return;
		if (!m_impl)
		{
			NENE_LOG_ERROR("Cannot execute Lua command: script engine is not initialized");
			return;
		}

		auto& lua = m_impl->lua;
		sol::object selectedFields = sol::make_object(lua, sol::nil);
		const auto* schema = selectedScript ? GetFieldSchema(selectedScript->scriptPath) : nullptr;
		if (schema != nullptr)
		{
			auto fields = lua.create_table();
			for (const auto& field : *schema)
			{
				const auto saved = selectedScript->fields.find(field.name);
				const auto& value = saved != selectedScript->fields.end() && IsValidScriptFieldValue(field, saved->second)
				                        ? saved->second
				                        : field.defaultValue;
				std::visit([&](const auto& fieldValue) { fields.set(field.name, fieldValue); }, value);
			}
			selectedFields = std::move(fields);
		}

		NENE_LOG_INFO("> {}", command);
		const LuaInstructionLimit instructionLimit(lua.lua_state(), kConsoleInstructionLimit);
		const auto result = m_impl->executeConsoleCommand(command, selectedFields);
		if (!result.valid())
		{
			const sol::error error = result;
			NENE_LOG_ERROR("{}", error.what());
			return;
		}
		if (result.return_count() > 0) NENE_LOG_INFO("{}", result.get<std::string>());
	}

	const sol::table* ScriptEngine::LoadScript(const std::filesystem::path& path)
	{
		if (!m_impl)
		{
			NENE_LOG_ERROR("Cannot load script '{}': script engine is not initialized", path.string());
			return nullptr;
		}
		return m_impl->LoadScript(path);
	}

	void ScriptEngine::ReloadScript(const std::filesystem::path& path)
	{
		if (!m_impl) return;
		const auto key = path.lexically_normal();
		if (m_impl->scripts.contains(key))
			m_impl->ReloadScripts(false, &key);
		else
			m_impl->LoadScript(key);
	}

	void ScriptEngine::ReloadAll()
	{
		if (!m_impl) return;
		NENE_LOG_INFO("Reloading tracked Lua scripts");
		m_impl->ReloadScripts(true);
	}

	void ScriptEngine::ReloadChangedScripts()
	{
		if (m_impl) m_impl->ReloadScripts(false);
	}

	bool ScriptEngine::HasScriptLoadErrors() const
	{
		return m_impl && std::ranges::any_of(m_impl->scripts, [](const auto& entry) { return entry.second.loadFailed; });
	}

	std::uint64_t ScriptEngine::GetScriptRevision(const std::filesystem::path& path) const
	{
		if (!m_impl) return 0;
		const auto it = m_impl->scripts.find(path.lexically_normal());
		return it != m_impl->scripts.end() ? it->second.revision : 0;
	}

	std::uint64_t ScriptEngine::GetScriptCacheGeneration() const
	{
		return m_impl ? m_impl->scriptCacheGeneration : 0;
	}

	const ScriptFieldSchema* ScriptEngine::GetFieldSchema(const std::filesystem::path& path) const
	{
		if (!m_impl) return nullptr;
		const auto it = m_impl->scripts.find(path.lexically_normal());
		if (it == m_impl->scripts.end() || !it->second.loaded) return nullptr;
		return &it->second.loaded->fieldSchema;
	}

	const sol::table* ScriptEngine::Impl::LoadScript(const std::filesystem::path& path)
	{
		const auto scriptPath = path.lexically_normal();
		auto [it, inserted] = scripts.try_emplace(scriptPath);
		auto& script = it->second;
		if (inserted)
		{
			if (IsValidScriptPath(scriptPath))
				LoadTrackedScript(scriptPath, script, ReadLastWriteTime(scriptsRoot / scriptPath), false);
			else
				NENE_LOG_ERROR("Invalid script path '{}': expected a path relative to assets/scripts", path.string());
		}
		return script.loaded ? &script.loaded->scriptClass : nullptr;
	}

	void ScriptEngine::Impl::ReloadScripts(bool force, const std::filesystem::path* target)
	{
		bool moduleChanged = false;
		for (auto& [name, module] : modules)
		{
			const auto writeTime = ReadLastWriteTime(module.path);
			moduleChanged |= writeTime != module.lastAttemptedWriteTime;
			module.lastAttemptedWriteTime = writeTime;
		}

		if (moduleChanged || (force && !modules.empty()))
		{
			ReloadModules(force, target);
			return;
		}

		for (auto& [path, script] : scripts)
		{
			if (!IsValidScriptPath(path)) continue;
			const auto writeTime = ReadLastWriteTime(scriptsRoot / path);
			if (target ? *target != path : !force && writeTime == script.lastAttemptedWriteTime) continue;
			if (moduleReloadPending)
			{
				ReloadModules(force, target);
				return;
			}
			LoadTrackedScript(path, script, writeTime, true);
		}
	}

	void ScriptEngine::Impl::ReloadModules(bool force, const std::filesystem::path* target)
	{
		// Stage behaviours that use require together so their closures share one generation of file modules.
		auto cache = lua.registry().get<sol::table>(LUA_LOADED_TABLE);
		std::unordered_map<std::string, sol::object> previousModules;
		for (const auto& [name, module] : modules)
		{
			previousModules.emplace(name, cache.raw_get<sol::object>(name));
			cache.raw_set(name, sol::lua_nil);
		}

		struct PendingScript
		{
			const std::filesystem::path* path;
			TrackedScript* script;
			LoadedScript loaded;
		};
		std::vector<PendingScript> pending;
		pending.reserve(scripts.size());
		moduleReloadPending = false;
		for (auto& [path, script] : scripts)
		{
			if (!IsValidScriptPath(path)) continue;
			const auto writeTime = ReadLastWriteTime(scriptsRoot / path);
			const bool usedRequire = script.loaded && script.loaded->usesRequire;
			const bool changed = writeTime != script.lastAttemptedWriteTime;
			const bool retry = !script.loaded && script.loadFailed;
			if (!usedRequire && !force && !(target && *target == path) && !changed && !retry) continue;

			script.lastAttemptedWriteTime = writeTime;
			auto loaded = TryLoadScript(path);
			script.loadFailed = !loaded;
			if (loaded)
			{
				if (usedRequire || loaded->usesRequire)
					pending.push_back({&path, &script, std::move(*loaded)});
				else
					ApplyScript(path, script, std::move(*loaded), true);
			}
			else if (usedRequire)
				moduleReloadPending = true;
		}

		if (moduleReloadPending)
		{
			for (const auto& [name, module] : modules)
			{
				const auto previous = previousModules.find(name);
				if (previous == previousModules.end())
					cache.raw_set(name, sol::lua_nil);
				else
					cache.raw_set(name, previous->second);
			}
			NENE_LOG_WARN("Keeping the last working Lua modules and behaviours");
			return;
		}

		for (auto& entry : pending)
			ApplyScript(*entry.path, *entry.script, std::move(entry.loaded), true);
	}

	void ScriptEngine::Impl::ApplyScript(const std::filesystem::path& path, TrackedScript& script,
	                                     LoadedScript loaded, bool isReload)
	{
		script.loaded = std::move(loaded);
		++script.revision;
		++scriptCacheGeneration;
		if (isReload) NENE_LOG_INFO("Reloaded Lua script '{}'", path.string());
	}

	void ScriptEngine::Impl::LoadTrackedScript(const std::filesystem::path& path, TrackedScript& script,
	                                         eastl::optional<std::filesystem::file_time_type> writeTime, bool isReload)
	{
		script.lastAttemptedWriteTime = writeTime;
		auto loaded = TryLoadScript(path);
		script.loadFailed = !loaded;
		if (loaded)
			ApplyScript(path, script, std::move(*loaded), isReload);
		else if (script.loaded)
		{
			NENE_LOG_WARN("Keeping the last working version of Lua script '{}'", path.string());
		}
	}

	eastl::optional<ScriptEngine::Impl::LoadedScript> ScriptEngine::Impl::TryLoadScript(const std::filesystem::path& path)
	{
		const ScriptExecutionContext::LoadScope loading(execution);
		const auto fileName = (scriptsRoot / path).string();
		sol::environment environment(lua, sol::create, lua.globals());
		const LuaInstructionLimit instructionLimit(lua.lua_state(), kScriptLoadInstructionLimit);
		scriptUsesRequire = false;
		const auto result =
		    lua.safe_script_file(fileName, environment, sol::script_pass_on_error, sol::load_mode::text);
		if (!result.valid())
		{
			const sol::error error = result;
			NENE_LOG_ERROR("Failed to load script '{}': {}", fileName, error.what());
			return eastl::nullopt;
		}

		if (result.return_count() == 0 || result.get_type() != sol::type::table)
		{
			NENE_LOG_ERROR("Script '{}' must return a table", fileName);
			return eastl::nullopt;
		}

		auto scriptClass = result.get<sol::table>();
		auto fieldSchema = ReadFieldSchema(scriptClass, fileName);
		if (!fieldSchema)
		{
			NENE_LOG_ERROR("Script '{}': invalid field schema or metadata", fileName);
			return eastl::nullopt;
		}

		return LoadedScript{std::move(environment), std::move(scriptClass), std::move(*fieldSchema), scriptUsesRequire};
	}

} // namespace NeneEngine
