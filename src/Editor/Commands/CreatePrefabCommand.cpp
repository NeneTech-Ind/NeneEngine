#include "Editor/Commands/CreatePrefabCommand.h"

#include "ECS/Components/HierarchyComponent.h"
#include "ECS/World.h"

#include <EASTL/vector.h>
#include <stdexcept>

namespace NeneEngine
{

	CreatePrefabCommand::CreatePrefabCommand(const ECS::World& world, ECS::Entity root) : m_root(root)
	{
		const auto* hierarchy = world.GetRegistry().try_get<ECS::HierarchyComponent>(root);
		if (hierarchy != nullptr && hierarchy->parent != ECS::NullEntity)
			throw std::runtime_error("Cannot record a prefab instance with a parent");
		auto serialized = SerializeEntityHierarchy(world, root);
		m_entities = std::move(serialized.entitiesById);
		m_data = std::move(serialized.entities);
	}

	void CreatePrefabCommand::Execute(ECS::World& world)
	{
		eastl::vector<ECS::Entity> created;
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
