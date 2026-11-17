#include "Editor/Commands/CreatePrefabCommand.h"

#include "ECS/Components/HierarchyComponent.h"
#include "ECS/World.h"

#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace NeneEngine
{

	CreatePrefabCommand::CreatePrefabCommand(const ECS::World& world, ECS::Entity root) : m_root(root)
	{
		std::vector<ECS::Entity> pending{root};
		std::unordered_set<ECS::Entity> visited;
		EntitySerializationMap ids;
		for (size_t next = 0; next < pending.size(); ++next)
		{
			const auto entity = pending[next];
			if (!world.GetRegistry().valid(entity) || !visited.insert(entity).second)
				throw std::runtime_error("Cannot record an invalid prefab hierarchy");
			const auto id = static_cast<uint32_t>(next + 1);
			ids.emplace(entity, id);
			m_entities.emplace(id, entity);
			if (const auto* hierarchy = world.GetRegistry().try_get<ECS::HierarchyComponent>(entity))
				pending.insert(pending.end(), hierarchy->children.begin(), hierarchy->children.end());
		}
		m_data = nlohmann::json::array();
		for (const auto entity : pending) m_data.push_back(SerializeEntity(world, entity, ids));
	}

	void CreatePrefabCommand::Execute(ECS::World& world)
	{
		std::vector<ECS::Entity> created;
		created.reserve(m_entities.size());
		try
		{
			for (const auto& record : m_data)
			{
				const auto expected = m_entities.at(record.at("id").get<uint32_t>());
				if (world.GetRegistry().valid(expected)) throw std::runtime_error("Prefab entity already exists");
				const auto entity = world.GetRegistry().create(expected);
				created.push_back(entity);
				if (entity != expected) throw std::runtime_error("Could not restore a prefab entity handle");
			}
			for (const auto& record : m_data) DeserializeEntity(record, world, m_entities);
		}
		catch (...)
		{
			for (auto it = created.rbegin(); it != created.rend(); ++it)
				if (world.GetRegistry().valid(*it)) world.GetRegistry().destroy(*it);
			throw;
		}
	}

	void CreatePrefabCommand::Undo(ECS::World& world)
	{
		if (world.GetRegistry().valid(m_root)) world.DestroyEntity(m_root);
	}

}
