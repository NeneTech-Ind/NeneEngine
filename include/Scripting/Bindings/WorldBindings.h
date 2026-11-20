#pragma once

#include <sol/forward.hpp>

namespace NeneEngine
{

	namespace ECS
	{
		class World;
	}

	class PrefabLibrary;
	class ScriptExecutionContext;

	void BindWorldApi(sol::table& nene, ECS::World& world, PrefabLibrary& prefabs,
	                  const ScriptExecutionContext& execution);

}
