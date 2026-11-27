// MovementSystem.h

#pragma once

#include "ECS/Entity.h"
#include "ECS/Systems/ISystem.h"

#include <EASTL/vector.h>

namespace NeneEngine
{
	class JobSystem;
}

namespace NeneEngine::ECS
{

	struct MovementComponent;
	struct TransformComponent;

	class MovementSystem final : public ISystem
	{
	  public:
		explicit MovementSystem(JobSystem& jobSystem);

		void Update(World& world, float deltaTime) override;

	  private:
		struct MovingEntity
		{
			Entity entity;
			TransformComponent* transform;
			MovementComponent* movement;
			bool positionChanged = false;
		};

		JobSystem& m_jobSystem;
		eastl::vector<MovingEntity> m_entities;
	};

} // namespace NeneEngine::ECS
