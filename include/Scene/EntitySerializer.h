#pragma once

#include "ECS/Entity.h"

#include <EASTL/unordered_map.h>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace NeneEngine
{

	namespace ECS
	{
		struct ScriptComponent;
		class World;
	}

	using EntitySerializationMap = eastl::unordered_map<ECS::Entity, uint32_t>;
	using EntityDeserializationMap = eastl::unordered_map<uint32_t, ECS::Entity>;

	struct SerializedEntityHierarchy
	{
		nlohmann::json entities;
		EntityDeserializationMap entitiesById;
	};

	[[nodiscard]] nlohmann::json SerializeScriptComponent(const ECS::ScriptComponent& script,
	                                                       const EntitySerializationMap* entityIds = nullptr);
	[[nodiscard]] nlohmann::json SerializeEntity(const ECS::World& world, ECS::Entity entity,
	                                             const EntitySerializationMap& entityIds);
	[[nodiscard]] SerializedEntityHierarchy SerializeEntityHierarchy(const ECS::World& world, ECS::Entity root);
	void DeserializeEntity(const nlohmann::json& entityJson, ECS::World& world,
	                       const EntityDeserializationMap& entitiesById);

}
