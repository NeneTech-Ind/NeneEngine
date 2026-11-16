#pragma once

#include "ECS/Entity.h"

#include <EASTL/unordered_map.h>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace NeneEngine
{

	namespace ECS
	{
		class World;
	}

	using EntitySerializationMap = eastl::unordered_map<ECS::Entity, uint32_t>;
	using EntityDeserializationMap = eastl::unordered_map<uint32_t, ECS::Entity>;

	[[nodiscard]] nlohmann::json SerializeEntity(const ECS::World& world, ECS::Entity entity,
	                                             const EntitySerializationMap& entityIds);
	void DeserializeEntity(const nlohmann::json& entityJson, ECS::World& world,
	                       const EntityDeserializationMap& entitiesById);

}
