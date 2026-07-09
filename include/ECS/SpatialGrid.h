// SpatialGrid.h

#pragma once

#include "ECS/Entity.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NeneEngine::ECS
{

	struct SpatialBounds
	{
		glm::vec3 min = {0.0f, 0.0f, 0.0f};
		glm::vec3 max = {0.0f, 0.0f, 0.0f};
	};

	class SpatialGrid
	{
	  public:
		explicit SpatialGrid(float cellSize = 25.0f);

		void Clear();
		void Insert(Entity entity, const SpatialBounds& bounds);
		[[nodiscard]] std::vector<Entity> Query(const SpatialBounds& bounds) const;
		[[nodiscard]] float GetCellSize() const noexcept { return m_cellSize; }

	  private:
		struct CellCoord
		{
			int32_t x = 0;
			int32_t y = 0;
			int32_t z = 0;

			bool operator==(const CellCoord& other) const noexcept
			{
				return x == other.x && y == other.y && z == other.z;
			}
		};

		struct CellCoordHash
		{
			std::size_t operator()(const CellCoord& coord) const noexcept;
		};

		struct Entry
		{
			Entity entity = NullEntity;
			SpatialBounds bounds{};
		};

		struct CellRange
		{
			CellCoord min{};
			CellCoord max{};
		};

		[[nodiscard]] CellCoord ToCellCoord(const glm::vec3& point) const;
		[[nodiscard]] CellRange ToCellRange(const SpatialBounds& bounds) const;

		float m_cellSize = 25.0f;
		std::vector<Entry> m_entries;
		std::unordered_map<CellCoord, std::vector<std::size_t>, CellCoordHash> m_cells;
	};

	[[nodiscard]] bool Intersects(const SpatialBounds& left, const SpatialBounds& right) noexcept;

} // namespace NeneEngine::ECS
