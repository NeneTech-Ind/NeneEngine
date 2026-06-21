// CollisionSystem.h

#pragma once

#include "ECS/Systems/ISystem.h"

namespace NeneEngine::ECS
{

	class CollisionSystem final : public ISystem
	{
	  public:
		void Update(World& world, float deltaTime) override;
	};

} // namespace NeneEngine::ECS
