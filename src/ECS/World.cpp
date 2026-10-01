// World.cpp

#include "ECS/World.h"
#include "ECS/Components/TagComponent.h"

namespace NeneEngine::ECS
{

	World::World() = default;

	Entity World::CreateEntity(const std::string& name)
	{
		Entity entity = m_registry.create();

		if (!name.empty()) m_registry.emplace<TagComponent>(entity, name);

		return entity;
	}

	void World::DestroyEntity(Entity entity)
	{
		if (m_registry.valid(entity)) m_registry.destroy(entity);
	}

	void World::AddSystem(eastl::unique_ptr<ISystem> system)
	{
		m_systems.push_back(std::move(system));
	}

	void World::Update(float deltaTime)
	{
		for (auto& system : m_systems) system->Update(*this, deltaTime);
	}

	void World::Render()
	{
		for (auto& system : m_systems) system->Render(*this);
	}

} // namespace NeneEngine::ECS
