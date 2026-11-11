// MovementSystem.cpp

#include "ECS/Systems/MovementSystem.h"
#include "Core/JobSystem.h"
#include "Core/Profiler.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

#include <cmath>

namespace NeneEngine::ECS
{

	namespace
	{
		constexpr uint32_t kMinJobRange = 256;

		void UpdateEntity(TransformComponent& transform, MovementComponent& movement, float deltaTime)
		{
			if (movement.useOscillation && movement.oscillationAmplitude > 0.0f)
			{
				const float axisLengthSquared = glm::dot(movement.oscillationAxis, movement.oscillationAxis);
				if (axisLengthSquared > 0.0001f)
				{
					movement.elapsedTime += deltaTime;
					const glm::vec3 axis = movement.oscillationAxis / std::sqrt(axisLengthSquared);
					const float offset =
					    std::sin(movement.elapsedTime * movement.oscillationSpeed) * movement.oscillationAmplitude;
					transform.position = movement.origin + axis * offset;
					return;
				}
			}

			transform.position += movement.velocity * deltaTime;
		}
	} // namespace

	MovementSystem::MovementSystem(JobSystem& jobSystem) : m_jobSystem(jobSystem)
	{
	}

	void MovementSystem::Update(World& world, float deltaTime)
	{
		NENE_PROFILE_SCOPE("Movement");
		auto view = world.GetRegistry().view<TransformComponent, MovementComponent>();

		m_entities.clear();
		for (auto [entity, transform, movement] : view.each())
			m_entities.push_back({&transform, &movement});

		const auto count = static_cast<uint32_t>(m_entities.size());
		const auto function = [this, deltaTime](uint32_t begin, uint32_t end)
		{
			for (uint32_t index = begin; index < end; ++index)
				UpdateEntity(*m_entities[index].transform, *m_entities[index].movement, deltaTime);
		};

		if (count > kMinJobRange)
		{
			const JobHandle job = m_jobSystem.ParallelFor("Movement", count, kMinJobRange, function);
			if (job.IsValid())
			{
				m_jobSystem.Wait(job);
				return;
			}
		}

		function(0, count);
	}

} // namespace NeneEngine::ECS
