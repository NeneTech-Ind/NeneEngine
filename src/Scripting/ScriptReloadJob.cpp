#include "Scripting/ScriptReloadJob.h"

#include "Core/Profiler.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <lua.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace NeneEngine
{
	namespace
	{
		std::filesystem::path FindModulePath(std::string_view searchPath, std::string name)
		{
			std::ranges::replace(name, '.', '/');
			while (!searchPath.empty())
			{
				const auto separator = searchPath.find(';');
				const auto pattern = searchPath.substr(0, separator);
				if (!pattern.empty())
				{
					std::string candidate;
					for (const char character : pattern)
					{
						if (character == '?')
							candidate += name;
						else
							candidate += character;
					}
					std::error_code error;
					if (std::filesystem::is_regular_file(candidate, error)) return candidate;
				}
				if (separator == std::string_view::npos) break;
				searchPath.remove_prefix(separator + 1);
			}
			return {};
		}

		int WriteBytecode(lua_State*, const void* data, size_t size, void* output)
		{
			try
			{
				auto& bytecode = *static_cast<std::vector<char>*>(output);
				const auto* bytes = static_cast<const char*>(data);
				bytecode.insert(bytecode.end(), bytes, bytes + size);
				return 0;
			}
			catch (...)
			{
				return 1;
			}
		}

		void CompileScript(lua_State* compiler, PreparedScriptChunk& chunk)
		{
			const auto fileName = chunk.path.string();
			NENE_PROFILE_SCOPE("Lua Script Compile");
			NENE_PROFILE_ZONE_TEXT(fileName.data(), fileName.size());
			std::ifstream file(chunk.path, std::ios::binary);
			if (!file)
			{
				chunk.error = "Could not open script file";
				return;
			}

			std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (file.bad())
			{
				chunk.error = "Could not read script file";
				return;
			}

			// Match Lua's text file loader and keep the original source line numbers.
			if (source.starts_with("\xEF\xBB\xBF")) source.erase(0, 3);
			if (source.starts_with('#'))
			{
				const auto lineEnd = source.find('\n');
				source.erase(0, lineEnd);
			}

			const auto chunkName = "@" + fileName;
			if (luaL_loadbufferx(compiler, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK)
			{
				const char* error = lua_tostring(compiler, -1);
				chunk.error = error ? error : "Lua compilation failed";
			}
			else if (lua_dump(compiler, WriteBytecode, &chunk.bytecode, 0) != 0)
			{
				chunk.error = "Could not write compiled Lua bytecode";
				chunk.bytecode.clear();
			}
			lua_pop(compiler, 1);
		}
	}

	ScriptWriteTime ReadScriptWriteTime(const std::filesystem::path& path)
	{
		std::error_code error;
		const auto writeTime = std::filesystem::last_write_time(path, error);
		if (error) return eastl::nullopt;
		return writeTime;
	}

	ScriptReloadResult PrepareScriptReload(const ScriptReloadRequest& request)
	{
		NENE_PROFILE_SCOPE("Lua Reload Prepare");
		ScriptReloadResult result;
		bool moduleChanged = false;
		for (const auto& [name, module] : request.modules)
		{
			const auto path = FindModulePath(request.moduleSearchPath, name);
			const auto writeTime = ReadScriptWriteTime(path);
			moduleChanged |= path != module.path || writeTime != module.lastAttemptedWriteTime;
			result.modules.emplace(name, PreparedScriptChunk{path, writeTime});
		}

		for (const auto& script : request.scripts)
		{
			const auto path = request.scriptsRoot / script.path;
			const auto writeTime = ReadScriptWriteTime(path);
			const bool targeted = std::ranges::find(request.options.targets, script.path) != request.options.targets.end();
			if (request.options.forceAll || targeted || writeTime != script.lastAttemptedWriteTime)
				result.scripts.emplace(script.path, PreparedScriptChunk{path, writeTime});
		}

		result.reloadModules = moduleChanged ||
		                       (!request.modules.empty() && request.options.forceAll) ||
		                       (request.retryModuleReload && !result.scripts.empty());
		if (result.reloadModules)
		{
			for (const auto& script : request.scripts)
			{
				if ((!script.usesRequire && !script.retryOnModuleReload) || result.scripts.contains(script.path))
					continue;
				const auto path = request.scriptsRoot / script.path;
				result.scripts.emplace(script.path, PreparedScriptChunk{path, ReadScriptWriteTime(path)});
			}
		}
		else
		{
			result.modules.clear();
		}

		if (result.scripts.empty() && result.modules.empty()) return result;
		const std::unique_ptr<lua_State, decltype(&lua_close)> compiler(luaL_newstate(), &lua_close);
		if (!compiler) throw std::runtime_error("Could not initialize the Lua reload compiler");

		for (auto& [key, chunk] : result.scripts)
			CompileScript(compiler.get(), chunk);
		for (auto& [name, chunk] : result.modules)
		{
			if (chunk.path.empty())
				chunk.error = "Lua module '" + name + "' could not be found";
			else
				CompileScript(compiler.get(), chunk);
		}

		for (const auto& [key, chunk] : result.scripts)
			result.sourcesChangedDuringRead |= chunk.writeTime != ReadScriptWriteTime(chunk.path);
		for (const auto& [name, chunk] : result.modules)
			result.sourcesChangedDuringRead |= chunk.writeTime != ReadScriptWriteTime(chunk.path);
		return result;
	}

} // namespace NeneEngine
