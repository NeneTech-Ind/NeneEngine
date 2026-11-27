// World.cpp

#include "ECS/World.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "Graphics/Runtime/GeometryBounds.h"

#include <chrono>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr size_t kMaxDeferredDestroyRequestsPerUpdate = 256;
		constexpr auto kDeferredDestructionBudget = std::chrono::milliseconds{2};
	}

	World::World() : m_lifetime(std::make_shared<WorldLifetime>())
	{
		m_lifetime->world = this;
		ConnectVisibilityObservers();
		m_eventBus.Subscribe<SceneRestoredEvent>(
		    [this](const SceneRestoredEvent&)
		    {
			    ++m_lifetime->sceneGeneration;
			    m_pendingDestruction.clear();
			    m_playStopRequested = false;
			    InvalidateVisibility();
			    for (auto& system : m_systems) system->OnSceneRestored(*this);
		    });
	}

	World::~World()
	{
		m_lifetime->world = nullptr;
	}

	void World::ConnectVisibilityObservers()
	{
		m_visibilityObservers = {
		    m_registry.on_construct<TransformComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		    m_registry.on_update<TransformComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		    m_registry.on_destroy<TransformComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		    m_registry.on_construct<MeshRendererComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		    m_registry.on_update<MeshRendererComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		    m_registry.on_destroy<MeshRendererComponent>().connect<&World::OnVisibilityComponentChanged>(*this),
		};
	}

	void World::OnVisibilityComponentChanged(entt::registry&, Entity entity)
	{
		m_visibilityChanges.insert(entity);
	}

	void World::InvalidateVisibility()
	{
		m_visibilityRegistry.Clear();
		m_visibilityChanges.clear();
		m_rebuildVisibility = true;
	}

	void World::SwapSceneRegistry(World& preparedScene)
	{
		for (auto& observer : m_visibilityObservers) observer.release();
		for (auto& observer : preparedScene.m_visibilityObservers) observer.release();
		m_registry.swap(preparedScene.m_registry);
		ConnectVisibilityObservers();
		preparedScene.ConnectVisibilityObservers();
		preparedScene.InvalidateVisibility();
		m_eventBus.Publish(SceneRestoredEvent{});
	}

	void World::SyncVisibility()
	{
		if (m_rebuildVisibility)
		{
			for (const Entity entity : m_registry.view<const MeshRendererComponent, const TransformComponent>())
				SyncEntityVisibility(entity);
			m_rebuildVisibility = false;
		}
		else
		{
			if (m_visibilityChanges.empty()) return;
			for (const Entity entity : m_visibilityChanges) SyncEntityVisibility(entity);
		}
		m_visibilityChanges.clear();
	}

	void World::SyncEntityVisibility(Entity entity)
	{
		if (!m_registry.valid(entity))
		{
			(void)m_visibilityRegistry.Remove(entity);
			return;
		}

		const auto* transform = m_registry.try_get<TransformComponent>(entity);
		const auto* mesh = m_registry.try_get<MeshRendererComponent>(entity);
		if (transform == nullptr || mesh == nullptr || !mesh->visible)
		{
			(void)m_visibilityRegistry.Remove(entity);
			return;
		}

		(void)m_visibilityRegistry.Register(entity);
		const auto* hierarchy = m_registry.try_get<HierarchyComponent>(entity);
		if (!mesh->meshPath.empty() || (hierarchy != nullptr && hierarchy->parent != NullEntity))
		{
			(void)m_visibilityRegistry.UpdateBounds(entity, std::nullopt);
			return;
		}

		(void)m_visibilityRegistry.UpdateGeometryBounds(entity, GetPrimitiveLocalBounds(mesh->primitiveType),
		                                                transform->GetModelMatrix());
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
		if (m_pendingDestruction.empty()) return;

		const auto sceneGeneration = m_lifetime->sceneGeneration;
		const auto deadline = std::chrono::steady_clock::now() + kDeferredDestructionBudget;
		for (size_t processed = 0; processed < kMaxDeferredDestroyRequestsPerUpdate &&
		                           m_lifetime->sceneGeneration == sceneGeneration && !m_pendingDestruction.empty();
		     ++processed)
		{
			const Entity entity = m_pendingDestruction.front();
			m_pendingDestruction.pop_front();
			DestroyEntity(entity);
			if (std::chrono::steady_clock::now() >= deadline) break;
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
		m_playStopRequested = false;
		for (auto& system : m_systems) system->OnPlayStarted(*this);
	}

	void World::NotifyPlayStopping()
	{
		for (auto& system : m_systems) system->OnPlayStopping(*this);
	}

} // namespace NeneEngine::ECS
