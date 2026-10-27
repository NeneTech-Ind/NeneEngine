// World.cpp

#include "ECS/World.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"

namespace NeneEngine::ECS
{

	World::World()
	{
		m_eventBus.Subscribe<SceneRestoredEvent>(
		    [this](const SceneRestoredEvent&)
		    {
			    for (auto& system : m_systems) system->OnSceneRestored(*this);
		    });
	}

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

} // namespace NeneEngine::ECS
