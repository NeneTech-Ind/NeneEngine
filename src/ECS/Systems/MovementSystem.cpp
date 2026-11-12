// MovementSystem.cpp

#include "ECS/Systems/MovementSystem.h"
#include "Core/JobSystem.h"
#include "Core/Profiler.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/MovementSettings.h"
#include "ECS/World.h"

#include <chrono>
#include <cmath>

namespace NeneEngine::ECS
{

	namespace
	{
		constexpr uint32_t kMinJobRange = 256;
		constexpr float kStatsSmoothing = 0.1f;

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
		const auto startTime = std::chrono::steady_clock::now();
		auto& registry = world.GetRegistry();
		const auto* settings = registry.ctx().find<MovementSettings>();
		const bool useJobs = settings == nullptr || settings->useJobSystem;
		auto view = registry.view<TransformComponent, MovementComponent>();

		m_entities.clear();
		for (auto [entity, transform, movement] : view.each())
			m_entities.push_back({&transform, &movement});

		const auto count = static_cast<uint32_t>(m_entities.size());
		const auto function = [this, deltaTime](uint32_t begin, uint32_t end)
		{
			for (uint32_t index = begin; index < end; ++index)
				UpdateEntity(*m_entities[index].transform, *m_entities[index].movement, deltaTime);
		};

		bool usedJobs = false;
		if (useJobs && count > kMinJobRange)
		{
			const JobHandle job = m_jobSystem.ParallelFor("Movement", count, kMinJobRange, function);
			if (job.IsValid())
			{
				m_jobSystem.Wait(job);
				usedJobs = true;
			}
		}
		if (!usedJobs) function(0, count);

		const float elapsedMs =
		    std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
		auto& stats = registry.ctx().emplace<MovementStats>();
		stats.updateMs = stats.usedJobSystem == useJobs && stats.updateMs > 0.0f
		                     ? stats.updateMs + (elapsedMs - stats.updateMs) * kStatsSmoothing
		                     : elapsedMs;
		stats.entityCount = count;
		stats.usedJobSystem = useJobs;
	}

} // namespace NeneEngine::ECS
