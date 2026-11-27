#include "Scripting/LuaBehaviour.h"

#include "Core/NeneLogger.h"
#include "Core/Profiler.h"
#include "Scripting/Bindings/ScriptEntity.h"
#include "Scripting/LuaInstructionLimit.h"

#include <algorithm>
#include <sol/sol.hpp>
#include <type_traits>
#include <utility>

namespace NeneEngine
{
	namespace
	{
		constexpr int kBehaviourInstructionLimit = 100'000;

		int LookupBehaviourField(lua_State* state)
		{
			lua_pushvalue(state, 2);
			lua_rawget(state, lua_upvalueindex(1));
			if (!lua_isnil(state, -1))
			{
				if (!sol::stack::get<const ScriptEntity&>(state, -1).IsValid()) lua_pushnil(state);
				return 1;
			}
			lua_pop(state, 1);

			// Delegate everything else to the class's original __index.
			lua_pushliteral(state, "__index");
			lua_rawget(state, lua_upvalueindex(2));
			if (lua_isfunction(state, -1))
			{
				lua_pushvalue(state, 1);
				lua_pushvalue(state, 2);
				lua_call(state, 2, 1);
			}
			else if (!lua_isnil(state, -1))
			{
				lua_pushvalue(state, 2);
				lua_gettable(state, -2);
			}
			return 1;
		}

		int InvokeBehaviourCallback(lua_State* state)
		{
			// This runs inside the protected call, so Lua code in __index is covered by the same budget
			lua_pushvalue(state, 2);
			lua_gettable(state, 1);
			if (lua_isnil(state, -1)) return 0;
			if (!lua_isfunction(state, -1))
				return luaL_error(state, "'%s' must be a function", lua_tostring(state, 2));

			lua_remove(state, 2);
			lua_insert(state, 1);
			lua_call(state, lua_gettop(state) - 1, 0);
			return 0;
		}
	}

	struct LuaBehaviour::Impl
	{
		ECS::World& world;
		sol::table instance;
		sol::table entityFields;
		sol::protected_function invokeCallback;
		std::string scriptPath;
		ScriptFieldSchema fieldSchema;
		bool started = false;
		bool destroyed = false;
		bool faulted = false;

		explicit Impl(ECS::World& world) : world(world) {}

		void SetClass(const sol::table& scriptClass, const ScriptFieldSchema& schema)
		{
			if (!std::ranges::any_of(schema, [](const auto& field)
			    { return std::holds_alternative<ScriptEntityReference>(field.defaultValue); }))
			{
				instance[sol::metatable_key] = scriptClass;
				return;
			}

			sol::state_view lua{instance.lua_state()};
			auto metatable = lua.create_table();
			for (const auto& [key, value] : scriptClass) metatable.raw_set(key, value);
			entityFields.push();
			scriptClass.push();
			lua_pushcclosure(lua.lua_state(), &LookupBehaviourField, 2);
			metatable.raw_set("__index", sol::stack::pop<sol::function>(lua.lua_state()));
			instance[sol::metatable_key] = metatable;
		}

		void ClearField(const std::string& name)
		{
			instance.raw_set(name, sol::lua_nil);
			if (entityFields.valid()) entityFields.raw_set(name, sol::lua_nil);
		}

		void SetField(const ScriptFieldDefinition& definition, const std::map<std::string, ScriptFieldValue>& fields)
		{
			const auto field = fields.find(definition.name);
			const ScriptFieldValue* value = &definition.defaultValue;
			if (field != fields.end())
			{
				if (IsValidScriptFieldValue(definition, field->second))
					value = &field->second;
				else
					NENE_LOG_WARN("Script '{}': invalid value for field '{}'; using its default", scriptPath,
					              definition.name);
			}
			if (const auto* reference = std::get_if<ScriptEntityReference>(value))
			{
				instance.raw_set(definition.name, sol::lua_nil);
				if (!entityFields.valid()) entityFields = sol::state_view(instance.lua_state()).create_table();
				entityFields.raw_set(definition.name, ScriptEntity(world, reference->target));
				return;
			}

			if (entityFields.valid()) entityFields.raw_set(definition.name, sol::lua_nil);
			std::visit([&](const auto& fieldValue)
			{
				if constexpr (!std::is_same_v<std::decay_t<decltype(fieldValue)>, ScriptEntityReference>)
					instance.raw_set(definition.name, fieldValue);
			}, *value);
		}

		[[nodiscard]] bool CanInvoke() const { return !destroyed && !faulted; }

		template <typename... Args> bool Invoke(std::string_view methodName, Args&&... args)
		{
			const LuaInstructionLimit instructionLimit(instance.lua_state(), kBehaviourInstructionLimit);
			const auto result = invokeCallback(instance, methodName, std::forward<Args>(args)...);
			if (!result.valid())
			{
				const sol::error error = result;
				NENE_LOG_ERROR("Script '{}': '{}' failed: {}", scriptPath, methodName, error.what());
				faulted = true;
				return false;
			}
			return true;
		}
	};

	LuaBehaviour::LuaBehaviour(ECS::World& world, ECS::Entity entity, const sol::table& scriptClass,
	                           std::string_view scriptPath, const ScriptFieldSchema& fieldSchema,
	                           const std::map<std::string, ScriptFieldValue>& fields)
	    : m_impl(std::make_unique<Impl>(world))
	{
		m_impl->scriptPath = scriptPath;
		sol::state_view lua{scriptClass.lua_state()};
		m_impl->instance = lua.create_table();
		m_impl->invokeCallback = sol::make_reference<sol::protected_function>(lua.lua_state(), &InvokeBehaviourCallback);
		// Initialize engine-owned fields without invoking a script's __newindex.
		for (const auto& definition : fieldSchema)
			m_impl->SetField(definition, fields);

		m_impl->instance.raw_set("entity", ScriptEntity(world, entity));
		m_impl->SetClass(scriptClass, fieldSchema);
		m_impl->fieldSchema = fieldSchema;
	}

	LuaBehaviour::~LuaBehaviour() = default;

	bool LuaBehaviour::HasStarted() const
	{
		return m_impl->started;
	}

	void LuaBehaviour::Reload(const sol::table& scriptClass, const ScriptFieldSchema& fieldSchema,
	                          const std::map<std::string, ScriptFieldValue>& fields)
	{
		for (const auto& previous : m_impl->fieldSchema)
		{
			if (previous.name != "entity" &&
			    std::ranges::find(fieldSchema, previous.name, &ScriptFieldDefinition::name) == fieldSchema.end())
				m_impl->ClearField(previous.name);
		}
		for (const auto& definition : fieldSchema)
		{
			if (definition.name == "entity") continue;
			const auto previous = std::ranges::find(m_impl->fieldSchema, definition.name, &ScriptFieldDefinition::name);
			if (previous == m_impl->fieldSchema.end() || previous->defaultValue != definition.defaultValue ||
			    previous->minimum != definition.minimum || previous->maximum != definition.maximum ||
			    previous->options != definition.options)
				m_impl->SetField(definition, fields);
		}
		m_impl->SetClass(scriptClass, fieldSchema);
		m_impl->fieldSchema = fieldSchema;
		m_impl->faulted = false;
	}

	void LuaBehaviour::OnStart()
	{
		if (m_impl->started || !m_impl->CanInvoke()) return;

		NENE_PROFILE_SCOPE("on_start");
		NENE_PROFILE_ZONE_TEXT(m_impl->scriptPath.data(), m_impl->scriptPath.size());
		m_impl->started = m_impl->Invoke("on_start");
	}

	void LuaBehaviour::OnUpdate(float deltaTime)
	{
		if (!m_impl->started || !m_impl->CanInvoke()) return;

		NENE_PROFILE_SCOPE("on_update");
		NENE_PROFILE_ZONE_TEXT(m_impl->scriptPath.data(), m_impl->scriptPath.size());
		m_impl->Invoke("on_update", deltaTime);
	}

	void LuaBehaviour::OnDestroy()
	{
		if (m_impl->destroyed) return;

		NENE_PROFILE_SCOPE("on_destroy");
		NENE_PROFILE_ZONE_TEXT(m_impl->scriptPath.data(), m_impl->scriptPath.size());
		// Destroy callback is allowed to run after an earlier callback faulted
		m_impl->Invoke("on_destroy");
		m_impl->destroyed = true;
	}

	void LuaBehaviour::OnCollision(const sol::object& other, const glm::vec3& normal)
	{
		if (!m_impl->started) OnStart();
		if (!m_impl->CanInvoke()) return;

		NENE_PROFILE_SCOPE("on_collision");
		NENE_PROFILE_ZONE_TEXT(m_impl->scriptPath.data(), m_impl->scriptPath.size());
		m_impl->Invoke("on_collision", other, normal);
	}

} // namespace NeneEngine
