// MovementSystem.h

#pragma once

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
			TransformComponent* transform;
			MovementComponent* movement;
		};

		JobSystem& m_jobSystem;
		eastl::vector<MovingEntity> m_entities;
	};

} // namespace NeneEngine::ECS
