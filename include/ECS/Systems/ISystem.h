// ISystem.h

#pragma once

#include "ECS/Entity.h"

namespace NeneEngine::ECS
{

	class World;

	class ISystem
	{
	  public:
		virtual ~ISystem() = default;

		virtual void Update(World& world, float deltaTime) = 0;
		virtual void OnSceneRestored(World& /*world*/) {}
		virtual void OnPlayStarted(World& /*world*/) {}
		virtual void OnPlayStopping(World& /*world*/) {}
		virtual void OnEntityDestroying(World& /*world*/, Entity /*entity*/) {}
	};

} // namespace NeneEngine::ECS
