#include "Scripting/Bindings/BehaviourBindings.h"

#include <glm/vec3.hpp>
#include <sol/sol.hpp>
#include <utility>

namespace NeneEngine
{

	void BindBehaviourApi(sol::table& nene)
	{
		auto behaviour = nene.create_named("Behaviour");
		behaviour["__index"] = behaviour;
		behaviour.set_function("on_start", [](sol::table) {});
		behaviour.set_function("on_update", [](sol::table, float) {});
		behaviour.set_function("on_destroy", [](sol::table) {});
		behaviour.set_function("on_collision", [](sol::table, sol::table, const glm::vec3&) {});
		behaviour.set_function("extend", [](const sol::table& base) {
			sol::state_view lua{base.lua_state()};
			sol::table derived = lua.create_table();
			derived["__index"] = derived;

			sol::table classMetatable = lua.create_table();
			classMetatable["__index"] = base;
			derived[sol::metatable_key] = classMetatable;
			return derived;
		});

		nene["Behaviour"] = std::move(behaviour);
	}

} // namespace NeneEngine
