#pragma once

#include <sol/forward.hpp>

namespace NeneEngine
{

	namespace ECS
	{
		class World;
	}

	class PrefabLibrary;

	void BindWorldApi(sol::table& nene, ECS::World& world, PrefabLibrary& prefabs);

}
