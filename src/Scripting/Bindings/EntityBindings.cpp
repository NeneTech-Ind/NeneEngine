#include "Scripting/Bindings/EntityBindings.h"

#include "Scripting/Bindings/ScriptEntity.h"
#include "Scripting/ScriptExecutionContext.h"

#include <sol/sol.hpp>

namespace NeneEngine
{

	void BindEntityApi(sol::table& nene, const ScriptExecutionContext& execution)
	{
		auto entity = nene.new_usertype<ScriptEntity>("Entity", sol::no_constructor);
		entity.set_function("is_valid", sol::protect([](const ScriptEntity& value) { return value.IsValid(); }));
		entity.set_function("name", sol::protect([](const ScriptEntity& value) { return value.GetName(); }));
		entity.set_function("get_position",
		                    sol::protect([](const ScriptEntity& value) { return value.GetPosition(); }));
		entity.set_function("set_position",
		                    sol::protect([&execution](const ScriptEntity& value, const glm::vec3& position)
		                                 {
			                                 execution.RequireWorldMutationAllowed("Entity:set_position");
			                                 value.SetPosition(position);
		                                 }));
		entity.set_function("get_rotation",
		                    sol::protect([](const ScriptEntity& value) { return value.GetRotation(); }));
		entity.set_function("set_rotation",
		                    sol::protect([&execution](const ScriptEntity& value, const glm::vec3& degrees)
		                                 {
			                                 execution.RequireWorldMutationAllowed("Entity:set_rotation");
			                                 value.SetRotation(degrees);
		                                 }));
		entity.set_function("get_velocity",
		                    sol::protect([](const ScriptEntity& value) { return value.GetVelocity(); }));
		entity.set_function("set_velocity",
		                    sol::protect([&execution](const ScriptEntity& value, const glm::vec3& velocity)
		                                 {
			                                 execution.RequireWorldMutationAllowed("Entity:set_velocity");
			                                 value.SetVelocity(velocity);
		                                 }));
		entity.set_function("is_grounded", sol::protect([](const ScriptEntity& value) { return value.IsGrounded(); }));
	}

}
