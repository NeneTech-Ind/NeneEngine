// VisibilityRegistry.cpp

#include "ECS/VisibilityRegistry.h"

#include <array>
#include <glm/geometric.hpp>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr float kFrustumBoundaryTolerance = 0.0001f;
		using FrustumPlanes = std::array<glm::vec4, 5>;

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
			if (!bounds || IntersectsFrustum(*bounds, planes, camera)) visibleEntities.push_back(entity);
		}
	}

	const std::optional<WorldAabb>* VisibilityRegistry::FindBounds(Entity entity) const
	{
		const auto entry = m_boundsByEntity.find(entity);
		return entry != m_boundsByEntity.end() ? &entry->second : nullptr;
	}

} // namespace NeneEngine::ECS
