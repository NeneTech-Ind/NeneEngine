// VisibilityRegistry.cpp

#include "ECS/VisibilityRegistry.h"

#include <array>
#include <glm/geometric.hpp>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr float kFrustumBoundaryTolerance = 0.0001f;
		using FrustumPlanes = std::array<glm::vec4, 6>;

		FrustumPlanes ExtractFrustumPlanes(const glm::mat4& viewProjection)
		{
			const glm::mat4 rows = glm::transpose(viewProjection);
			const glm::vec4& w = rows[3];
#if defined(GLM_FORCE_DEPTH_ZERO_TO_ONE)
			const glm::vec4 nearPlane = rows[2];
#else
			const glm::vec4 nearPlane = w + rows[2];
#endif
			FrustumPlanes planes{w + rows[0], w - rows[0], w + rows[1], w - rows[1], nearPlane, w - rows[2]};
			for (glm::vec4& plane : planes) plane /= glm::length(glm::vec3(plane));
			return planes;
		}

		bool IntersectsFrustum(const WorldAabb& bounds, const FrustumPlanes& planes)
		{
			for (const glm::vec4& plane : planes)
			{
				const glm::vec3 furthestCorner{plane.x >= 0.0f ? bounds.max.x : bounds.min.x,
				                               plane.y >= 0.0f ? bounds.max.y : bounds.min.y,
				                               plane.z >= 0.0f ? bounds.max.z : bounds.min.z};
				const float distance = glm::dot(glm::vec3(plane), furthestCorner) + plane.w;
				if (distance < -kFrustumBoundaryTolerance) return false;
			}
			return true;
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
			if (!bounds || IntersectsFrustum(*bounds, planes)) visibleEntities.push_back(entity);
		}
	}

	const std::optional<WorldAabb>* VisibilityRegistry::FindBounds(Entity entity) const
	{
		const auto entry = m_boundsByEntity.find(entity);
		return entry != m_boundsByEntity.end() ? &entry->second : nullptr;
	}

} // namespace NeneEngine::ECS
