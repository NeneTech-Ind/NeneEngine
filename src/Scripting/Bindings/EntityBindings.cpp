#include "Scripting/Bindings/EntityBindings.h"

#include "Scripting/Bindings/ScriptEntity.h"

#include <sol/sol.hpp>

namespace NeneEngine
{

	void BindEntityApi(sol::table& nene)
	{
		auto entity = nene.new_usertype<ScriptEntity>("Entity", sol::no_constructor);
		entity.set_function("is_valid", sol::protect([](const ScriptEntity& value) { return value.IsValid(); }));
		entity.set_function("name", sol::protect([](const ScriptEntity& value) { return value.GetName(); }));
		entity.set_function("get_position",
		                    sol::protect([](const ScriptEntity& value) { return value.GetPosition(); }));
		entity.set_function("set_position", sol::protect([](const ScriptEntity& value, const glm::vec3& position)
		                                                 { value.SetPosition(position); }));
		entity.set_function("get_rotation",
		                    sol::protect([](const ScriptEntity& value) { return value.GetRotation(); }));
		entity.set_function("set_rotation", sol::protect([](const ScriptEntity& value, const glm::vec3& degrees)
		                                                 { value.SetRotation(degrees); }));
		entity.set_function("get_velocity",
		                    sol::protect([](const ScriptEntity& value) { return value.GetVelocity(); }));
		entity.set_function("set_velocity", sol::protect([](const ScriptEntity& value, const glm::vec3& velocity)
		                                                 { value.SetVelocity(velocity); }));
		entity.set_function("is_grounded", sol::protect([](const ScriptEntity& value) { return value.IsGrounded(); }));
	}

}
