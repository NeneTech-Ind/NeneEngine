// VisibilityRegistry.h

#pragma once

#include "ECS/Entity.h"
#include "Graphics/Runtime/LocalAabb.h"

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <cstddef>
#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <optional>

namespace NeneEngine::ECS
{

	struct WorldAabb
	{
		glm::vec3 min{0.0f};
		glm::vec3 max{0.0f};
	};

	struct VisibilityCamera
	{
		VisibilityCamera(const glm::mat4& viewProjection, const glm::vec3& worldPosition, const glm::vec3& forward,
		                 float farPlane)
		    : viewProjection(viewProjection), worldPosition(worldPosition), forward(forward), farPlane(farPlane)
		{
		}

		glm::mat4 viewProjection;
		glm::vec3 worldPosition;
		glm::vec3 forward;
		float farPlane;
	};

	class VisibilityRegistry
	{
	  public:
		[[nodiscard]] bool Register(Entity entity, std::optional<WorldAabb> bounds = std::nullopt);
		[[nodiscard]] bool UpdateBounds(Entity entity, std::optional<WorldAabb> bounds);
		[[nodiscard]] bool UpdateGeometryBounds(Entity entity, const std::optional<LocalAabb>& localBounds,
		                                        const glm::mat4& worldMatrix);
		[[nodiscard]] bool Remove(Entity entity);
		void Clear();

		void QueryVisible(const VisibilityCamera& camera, eastl::vector<Entity>& visibleEntities) const;

		[[nodiscard]] const std::optional<WorldAabb>* FindBounds(Entity entity) const;
		[[nodiscard]] std::size_t Size() const { return m_boundsByEntity.size(); }

	  private:
		eastl::unordered_map<Entity, std::optional<WorldAabb>, std::hash<Entity>> m_boundsByEntity;
	};

} // namespace NeneEngine::ECS
