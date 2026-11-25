// VisibilityRegistry.h

#pragma once

#include "ECS/Entity.h"

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
		glm::mat4 viewProjection{1.0f};
	};

	class VisibilityRegistry
	{
	  public:
		[[nodiscard]] bool Register(Entity entity, std::optional<WorldAabb> bounds = std::nullopt);
		[[nodiscard]] bool UpdateBounds(Entity entity, std::optional<WorldAabb> bounds);
		[[nodiscard]] bool Remove(Entity entity);
		void Clear();

		void QueryVisible(const VisibilityCamera& camera, eastl::vector<Entity>& visibleEntities) const;

		[[nodiscard]] const std::optional<WorldAabb>* FindBounds(Entity entity) const;
		[[nodiscard]] std::size_t Size() const { return m_boundsByEntity.size(); }

	  private:
		eastl::unordered_map<Entity, std::optional<WorldAabb>, std::hash<Entity>> m_boundsByEntity;
	};

} // namespace NeneEngine::ECS
