// VisibilityRegistry.cpp

#include "ECS/VisibilityRegistry.h"

#include "Core/JobSystem.h"

#include <array>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr float kFrustumBoundaryTolerance = 0.0001f;
		constexpr uint32_t kMinVisibilityJobRange = 256;
		using FrustumPlanes = std::array<glm::vec4, 5>;

		struct VisibilityCandidate
		{
			Entity entity;
			std::optional<WorldAabb> bounds;
		};

		std::optional<WorldAabb> TransformBounds(const LocalAabb& local, const glm::mat4& worldMatrix)
		{
			constexpr float infinity = std::numeric_limits<float>::infinity();
			WorldAabb world{glm::vec3(infinity), glm::vec3(-infinity)};
			for (unsigned cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
			{
				const glm::vec4 corner{(cornerIndex & 1) ? local.max.x : local.min.x,
				                       (cornerIndex & 2) ? local.max.y : local.min.y,
				                       (cornerIndex & 4) ? local.max.z : local.min.z, 1.0f};
				const glm::vec3 position{worldMatrix * corner};
				if (glm::any(glm::isnan(position)) || glm::any(glm::isinf(position))) return std::nullopt;
				world.min = glm::min(world.min, position);
				world.max = glm::max(world.max, position);
			}
			return world;
		}

		FrustumPlanes ExtractFrustumPlanes(const glm::mat4& viewProjection)
		{
			const glm::mat4 rows = glm::transpose(viewProjection);
			const glm::vec4& w = rows[3];
#if defined(GLM_FORCE_DEPTH_ZERO_TO_ONE)
			const glm::vec4 nearPlane = rows[2];
#else
			const glm::vec4 nearPlane = w + rows[2];
#endif
			FrustumPlanes planes{w + rows[0], w - rows[0], w + rows[1], w - rows[1], nearPlane};
			for (glm::vec4& plane : planes) plane /= glm::length(glm::vec3(plane));
			return planes;
		}

		glm::vec3 FurthestCorner(const WorldAabb& bounds, const glm::vec3& direction)
		{
			return {direction.x >= 0.0f ? bounds.max.x : bounds.min.x,
			        direction.y >= 0.0f ? bounds.max.y : bounds.min.y,
			        direction.z >= 0.0f ? bounds.max.z : bounds.min.z};
		}

		bool IntersectsFrustum(const WorldAabb& bounds, const FrustumPlanes& planes, const VisibilityCamera& camera)
		{
			for (const glm::vec4& plane : planes)
			{
				const glm::vec3 normal{plane};
				const float distance = glm::dot(normal, FurthestCorner(bounds, normal)) + plane.w;
				if (distance < -kFrustumBoundaryTolerance) return false;
			}

			const glm::vec3 closestCorner = FurthestCorner(bounds, -camera.forward);
			const float closestDepth = glm::dot(camera.forward, closestCorner - camera.worldPosition);
			return closestDepth - camera.farPlane <= kFrustumBoundaryTolerance;
		}

		bool IsVisible(const std::optional<WorldAabb>& bounds, const FrustumPlanes& planes, const VisibilityCamera& camera)
		{
			return !bounds || IntersectsFrustum(*bounds, planes, camera);
		}
	} // namespace

	bool VisibilityRegistry::Register(Entity entity, std::optional<WorldAabb> bounds)
	{
		if (entity == NullEntity) return false;
		return m_boundsByEntity.insert({entity, bounds}).second;
	}

	bool VisibilityRegistry::UpdateBounds(Entity entity, std::optional<WorldAabb> bounds)
	{
		const auto entry = m_boundsByEntity.find(entity);
		if (entry == m_boundsByEntity.end()) return false;
		entry->second = bounds;
		return true;
	}

	bool VisibilityRegistry::UpdateGeometryBounds(Entity entity, const std::optional<LocalAabb>& localBounds,
	                                              const glm::mat4& worldMatrix)
	{
		return UpdateBounds(entity, localBounds ? TransformBounds(*localBounds, worldMatrix) : std::nullopt);
	}

	bool VisibilityRegistry::Remove(Entity entity)
	{
		return m_boundsByEntity.erase(entity) != 0;
	}

	void VisibilityRegistry::Clear()
	{
		m_boundsByEntity.clear();
	}

	void VisibilityRegistry::QueryVisible(const VisibilityCamera& camera, eastl::vector<Entity>& visibleEntities) const
	{
		visibleEntities.clear();
		const FrustumPlanes planes = ExtractFrustumPlanes(camera.viewProjection);
		for (const auto& [entity, bounds] : m_boundsByEntity)
		{
			if (IsVisible(bounds, planes, camera)) visibleEntities.push_back(entity);
		}
	}

	void VisibilityRegistry::QueryVisible(const VisibilityCamera& camera, eastl::vector<Entity>& visibleEntities,
	                                      JobSystem& jobs) const
	{
		if (m_boundsByEntity.size() <= kMinVisibilityJobRange || jobs.GetComputeWorkerCount() == 0)
		{
			QueryVisible(camera, visibleEntities);
			return;
		}

		eastl::vector<VisibilityCandidate> candidates;
		candidates.reserve(m_boundsByEntity.size());
		for (const auto& [entity, bounds] : m_boundsByEntity) candidates.push_back({entity, bounds});

		const FrustumPlanes planes = ExtractFrustumPlanes(camera.viewProjection);
		eastl::vector<uint8_t> visible(candidates.size());
		const auto checkRange = [&](uint32_t begin, uint32_t end)
		{
			for (uint32_t index = begin; index < end; ++index)
				visible[index] = IsVisible(candidates[index].bounds, planes, camera);
		};
		const JobHandle job = jobs.ParallelFor("Visibility Cull", static_cast<uint32_t>(candidates.size()),
		                                      kMinVisibilityJobRange, checkRange);
		if (job.IsValid()) jobs.Wait(job);
		if (!job.IsValid() || job.HasFailed())
		{
			QueryVisible(camera, visibleEntities);
			return;
		}

		visibleEntities.clear();
		for (std::size_t index = 0; index < candidates.size(); ++index)
		{
			if (visible[index]) visibleEntities.push_back(candidates[index].entity);
		}
	}

	const std::optional<WorldAabb>* VisibilityRegistry::FindBounds(Entity entity) const
	{
		const auto entry = m_boundsByEntity.find(entity);
		return entry != m_boundsByEntity.end() ? &entry->second : nullptr;
	}

} // namespace NeneEngine::ECS
