// SpatialGrid.cpp

#include "ECS/SpatialGrid.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace NeneEngine::ECS
{
	namespace
	{
		uint32_t ToEntityId(Entity entity)
		{
			return static_cast<uint32_t>(entt::to_integral(entity));
		}

		SpatialBounds NormalizeBounds(const SpatialBounds& bounds)
		{
			return SpatialBounds{glm::min(bounds.min, bounds.max), glm::max(bounds.min, bounds.max)};
		}
	} // namespace

	SpatialGrid::SpatialGrid(float cellSize) : m_cellSize((std::max)(cellSize, 0.001f)) {}

	void SpatialGrid::Clear()
	{
		m_entries.clear();
		m_cells.clear();
	}

	void SpatialGrid::Insert(Entity entity, const SpatialBounds& bounds)
	{
		const SpatialBounds normalizedBounds = NormalizeBounds(bounds);
		const std::size_t entryIndex = m_entries.size();
		m_entries.push_back(Entry{entity, normalizedBounds});

		const CellRange range = ToCellRange(normalizedBounds);
		for (int32_t z = range.min.z; z <= range.max.z; ++z)
		{
			for (int32_t y = range.min.y; y <= range.max.y; ++y)
			{
				for (int32_t x = range.min.x; x <= range.max.x; ++x)
				{
					m_cells[CellCoord{x, y, z}].push_back(entryIndex);
				}
			}
		}
	}

	std::vector<Entity> SpatialGrid::Query(const SpatialBounds& bounds) const
	{
		const SpatialBounds normalizedBounds = NormalizeBounds(bounds);
		const CellRange range = ToCellRange(normalizedBounds);

		std::vector<Entity> result;
		std::unordered_set<uint32_t> visitedEntities;
		std::unordered_set<std::size_t> visitedEntries;

		for (int32_t z = range.min.z; z <= range.max.z; ++z)
		{
			for (int32_t y = range.min.y; y <= range.max.y; ++y)
			{
				for (int32_t x = range.min.x; x <= range.max.x; ++x)
				{
					const auto cell = m_cells.find(CellCoord{x, y, z});
					if (cell == m_cells.end()) continue;

					for (const std::size_t entryIndex : cell->second)
					{
						if (!visitedEntries.insert(entryIndex).second) continue;

						const Entry& entry = m_entries[entryIndex];
						if (!Intersects(entry.bounds, normalizedBounds)) continue;

						if (visitedEntities.insert(ToEntityId(entry.entity)).second) result.push_back(entry.entity);
					}
				}
			}
		}

		return result;
	}

	std::size_t SpatialGrid::CellCoordHash::operator()(const CellCoord& coord) const noexcept
	{
		const std::size_t x = static_cast<std::size_t>(coord.x) * 73856093u;
		const std::size_t y = static_cast<std::size_t>(coord.y) * 19349663u;
		const std::size_t z = static_cast<std::size_t>(coord.z) * 83492791u;
		return x ^ y ^ z;
	}

	SpatialGrid::CellCoord SpatialGrid::ToCellCoord(const glm::vec3& point) const
	{
		return CellCoord{static_cast<int32_t>(std::floor(point.x / m_cellSize)),
		                 static_cast<int32_t>(std::floor(point.y / m_cellSize)),
		                 static_cast<int32_t>(std::floor(point.z / m_cellSize))};
	}

	SpatialGrid::CellRange SpatialGrid::ToCellRange(const SpatialBounds& bounds) const
	{
		return CellRange{ToCellCoord(bounds.min), ToCellCoord(bounds.max)};
	}

	bool Intersects(const SpatialBounds& left, const SpatialBounds& right) noexcept
	{
		return left.min.x <= right.max.x && left.max.x >= right.min.x && left.min.y <= right.max.y &&
		       left.max.y >= right.min.y && left.min.z <= right.max.z && left.max.z >= right.min.z;
	}

} // namespace NeneEngine::ECS
