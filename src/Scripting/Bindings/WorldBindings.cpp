#include "Scripting/Bindings/WorldBindings.h"

#include "ECS/World.h"
#include "Scripting/Bindings/ScriptEntity.h"

#include <sol/sol.hpp>

namespace NeneEngine
{

	void BindWorldApi(sol::table& nene)
	{
		auto worldApi = nene.create_named("world");
		worldApi.set_function("destroy", sol::protect([](const ScriptEntity& entity)
		                                              { entity.GetWorld().DeferDestroyEntity(entity.GetId()); }));
	}

}
