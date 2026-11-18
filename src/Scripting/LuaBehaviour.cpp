#include "Scripting/LuaBehaviour.h"

#include "Core/NeneLogger.h"
#include "Scripting/LuaInstructionLimit.h"

#include <sol/sol.hpp>
#include <utility>

namespace NeneEngine
{
	namespace
	{
		constexpr int kBehaviourInstructionLimit = 100'000;

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
		sol::table instance;
		sol::protected_function invokeCallback;
		std::string scriptPath;
		bool started = false;
		bool destroyed = false;
		bool faulted = false;

		template <typename... Args> void Invoke(std::string_view methodName, bool allowWhenFaulted, Args&&... args)
		{
			if (destroyed || (faulted && !allowWhenFaulted)) return;

			const LuaInstructionLimit instructionLimit(instance.lua_state(), kBehaviourInstructionLimit);
			const auto result = invokeCallback(instance, methodName, std::forward<Args>(args)...);
			if (!result.valid())
			{
				const sol::error error = result;
				NENE_LOG_ERROR("Script '{}': '{}' failed: {}", scriptPath, methodName, error.what());
				faulted = true;
			}
		}
	};

	LuaBehaviour::LuaBehaviour(const sol::table& scriptClass, std::string_view scriptPath,
	                           const ScriptFieldSchema& fieldSchema,
	                           const std::map<std::string, ScriptFieldValue>& fields, const sol::object& entityApi)
	    : m_impl(std::make_unique<Impl>())
	{
		m_impl->scriptPath = scriptPath;
		sol::state_view lua{scriptClass.lua_state()};
		m_impl->instance = lua.create_table();
		m_impl->invokeCallback = sol::make_reference<sol::protected_function>(lua.lua_state(), &InvokeBehaviourCallback);
		m_impl->instance[sol::metatable_key] = scriptClass;

		// Initialize engine-owned fields without invoking a script's __newindex.
		for (const auto& definition : fieldSchema)
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

			std::visit([&](const auto& fieldValue) { m_impl->instance.raw_set(definition.name, fieldValue); }, *value);
		}

		m_impl->instance.raw_set("entity", entityApi);
	}

	LuaBehaviour::~LuaBehaviour() = default;

	void LuaBehaviour::OnStart()
	{
		if (m_impl->started || m_impl->destroyed) return;
		m_impl->started = true;
		m_impl->Invoke("on_start", false);
	}

	void LuaBehaviour::OnUpdate(float deltaTime)
	{
		if (!m_impl->started || m_impl->destroyed) return;
		m_impl->Invoke("on_update", false, deltaTime);
	}

	void LuaBehaviour::OnDestroy()
	{
		if (m_impl->destroyed) return;
		// Destroy callback is allowed to run after an earlier callback faulted
		m_impl->Invoke("on_destroy", true);
		m_impl->destroyed = true;
	}

	void LuaBehaviour::OnCollision(const sol::object& other, const glm::vec3& normal)
	{
		if (!m_impl->started) OnStart();
		m_impl->Invoke("on_collision", false, other, normal);
	}

} // namespace NeneEngine
