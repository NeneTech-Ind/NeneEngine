#include "ECS/SpatialGrid.h"

#include <entt/entt.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

using namespace NeneEngine::ECS;

namespace
{
	Entity EntityFromId(uint32_t id)
	{
		return static_cast<Entity>(id);
	}

	bool Contains(const std::vector<Entity>& entities, Entity entity)
	{
		return std::find(entities.begin(), entities.end(), entity) != entities.end();
	}
} // namespace

int main()
{
	SpatialGrid grid(10.0f);

	const Entity nearEntity = EntityFromId(1);
	const Entity farEntity = EntityFromId(2);
	const Entity spanningEntity = EntityFromId(3);

	grid.Insert(nearEntity, SpatialBounds{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}});
	grid.Insert(farEntity, SpatialBounds{{100.0f, 100.0f, 100.0f}, {110.0f, 110.0f, 110.0f}});
	grid.Insert(spanningEntity, SpatialBounds{{8.0f, -1.0f, -1.0f}, {12.0f, 1.0f, 1.0f}});

	const auto nearby = grid.Query(SpatialBounds{{-2.0f, -2.0f, -2.0f}, {9.0f, 2.0f, 2.0f}});
	assert(Contains(nearby, nearEntity));
	assert(Contains(nearby, spanningEntity));
	assert(!Contains(nearby, farEntity));

	grid.Clear();
	const auto empty = grid.Query(SpatialBounds{{-200.0f, -200.0f, -200.0f}, {200.0f, 200.0f, 200.0f}});
	assert(empty.empty());

	return 0;
}
