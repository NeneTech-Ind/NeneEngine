#include "Scripting/LuaBehaviour.h"

#include "Core/NeneLogger.h"

#include <sol/sol.hpp>
#include <utility>

namespace NeneEngine
{

	struct LuaBehaviour::Impl
	{
		sol::table instance;
		std::string scriptPath;
		bool started = false;
		bool destroyed = false;
		bool faulted = false;

		template <typename... Args> void Invoke(std::string_view methodName, bool allowWhenFaulted, Args&&... args)
		{
			if (destroyed || (faulted && !allowWhenFaulted)) return;

			const sol::object callbackObject = instance.get<sol::object>(std::string{methodName});
			if (callbackObject == sol::nil) return;
			if (callbackObject.get_type() != sol::type::function)
			{
				NENE_LOG_ERROR("Script '{}': '{}' must be a function", scriptPath, methodName);
				faulted = true;
				return;
			}

			sol::protected_function callback = callbackObject.as<sol::protected_function>();
			auto result = callback(instance, std::forward<Args>(args)...);
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
		m_impl->instance[sol::metatable_key] = scriptClass;

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

			std::visit([&](const auto& fieldValue) { m_impl->instance.set(definition.name, fieldValue); }, *value);
		}

		m_impl->instance.set("entity", entityApi);
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
