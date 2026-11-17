// World.cpp

#include "ECS/World.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"

namespace NeneEngine::ECS
{

	World::World() : m_lifetime(std::make_shared<WorldLifetime>())
	{
		m_lifetime->world = this;
		m_eventBus.Subscribe<SceneRestoredEvent>(
		    [this](const SceneRestoredEvent&)
		    {
			    ++m_lifetime->sceneGeneration;
			    m_pendingDestruction.clear();
			    for (auto& system : m_systems) system->OnSceneRestored(*this);
		    });
	}

	World::~World()
	{
		m_lifetime->world = nullptr;
	}

	Entity World::CreateEntity(const std::string& name)
	{
		Entity entity = m_registry.create();

		if (!name.empty()) m_registry.emplace<TagComponent>(entity, name);

		return entity;
	}

	void World::DestroyEntity(Entity entity)
	{
		if (!m_registry.valid(entity)) return;

		DetachFromParent(entity);

		eastl::vector<Entity> subtree{entity};
		for (size_t index = 0; index < subtree.size(); ++index)
		{
			const Entity current = subtree[index];
			const auto* hierarchy = m_registry.try_get<HierarchyComponent>(current);
			if (hierarchy == nullptr) continue;

			for (const Entity child : hierarchy->children)
			{
				if (!m_registry.valid(child)) continue;

				const auto* childHierarchy = m_registry.try_get<HierarchyComponent>(child);
				if (childHierarchy != nullptr && childHierarchy->parent == current) subtree.push_back(child);
			}
		}

		for (auto it = subtree.rbegin(); it != subtree.rend(); ++it)
		{
			if (!m_registry.valid(*it)) continue;

			for (auto& system : m_systems) system->OnEntityDestroying(*this, *it);
			m_registry.destroy(*it);
		}
	}

	void World::DetachFromParent(Entity entity)
	{
		auto* hierarchy = m_registry.try_get<HierarchyComponent>(entity);
		if (hierarchy == nullptr) return;

		const Entity parent = hierarchy->parent;
		hierarchy->parent = NullEntity;
		if (!m_registry.valid(parent)) return;

		if (auto* parentHierarchy = m_registry.try_get<HierarchyComponent>(parent))
			eastl::erase(parentHierarchy->children, entity);
	}

	void World::DeferDestroyEntity(Entity entity)
	{
		if (m_registry.valid(entity)) m_pendingDestruction.push_back(entity);
	}

	void World::FlushDeferredDestruction()
	{
		const auto sceneGeneration = m_lifetime->sceneGeneration;
		while (m_lifetime->sceneGeneration == sceneGeneration && !m_pendingDestruction.empty())
		{
			eastl::vector<Entity> pending;
			pending.swap(m_pendingDestruction);
			for (const Entity entity : pending)
			{
				if (m_lifetime->sceneGeneration != sceneGeneration) return;
				DestroyEntity(entity);
			}
		}
	}

	void World::AddSystem(eastl::unique_ptr<ISystem> system)
	{
		m_systems.push_back(std::move(system));
	}

	void World::Update(float deltaTime)
	{
		for (auto& system : m_systems) system->Update(*this, deltaTime);
		FlushDeferredDestruction();
	}

	void World::NotifyPlayStarted()
	{
		for (auto& system : m_systems) system->OnPlayStarted(*this);
	}

	void World::NotifyPlayStopping()
	{
		for (auto& system : m_systems) system->OnPlayStopping(*this);
	}

} // namespace NeneEngine::ECS
