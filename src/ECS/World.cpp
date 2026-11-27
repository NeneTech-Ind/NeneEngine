// World.cpp

#include "ECS/World.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "Graphics/Runtime/GeometryBounds.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <EASTL/map.h>
#include <algorithm>
#include <chrono>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr size_t kMaxDeferredDestroyRequestsPerUpdate = 256;
		constexpr auto kDeferredDestructionBudget = std::chrono::milliseconds{2};

		Entity GetParent(const entt::registry& registry, Entity entity)
		{
			const auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
			return hierarchy != nullptr ? hierarchy->parent : NullEntity;
		}
	}

	class World::VisibilityMatrices
	{
	  public:
		explicit VisibilityMatrices(const entt::registry& registry) : m_registry(registry) {}

		std::optional<glm::mat4> Get(Entity entity)
		{
			m_path.clear();
			std::optional<glm::mat4> worldMatrix{glm::mat4(1.0f)};
			while (m_registry.all_of<TransformComponent>(entity))
			{
				const auto [entry, inserted] = m_matrices.try_emplace(entity);
				if (!inserted)
				{
					worldMatrix = entry->second;
					break;
				}
				m_path.push_back(entity);
				entity = GetParent(m_registry, entity);
			}

			for (auto it = m_path.rbegin(); it != m_path.rend(); ++it)
			{
				if (worldMatrix) *worldMatrix *= m_registry.get<TransformComponent>(*it).GetModelMatrix();
				m_matrices.find(*it)->second = worldMatrix;
			}
			return worldMatrix;
		}

	  private:
		const entt::registry& m_registry;
		eastl::unordered_map<Entity, std::optional<glm::mat4>, std::hash<Entity>> m_matrices;
		eastl::vector<Entity> m_path;
	};

	class World::VisibilityGeometry
	{
	  public:
		void SetSources(SceneAssetResolver& assets, eastl::span<IRenderAdapter* const> renderers, EntitySet& dirty)
		{
			const uint64_t retryRevision = assets.GetRetryRevision();
			if (m_assets != &assets ||
			    !std::equal(m_renderers.begin(), m_renderers.end(), renderers.begin(), renderers.end()))
			{
				m_assets = &assets;
				m_renderers.assign(renderers.begin(), renderers.end());
				m_failed.clear();
				for (auto& binding : m_bindings)
				{
					binding.second.bounds.reset();
					m_pending.insert(&binding);
					dirty.insert(binding.second.entities.begin(), binding.second.entities.end());
				}
			}
			else if (m_retryRevision != retryRevision)
			{
				m_pending.insert(m_failed.begin(), m_failed.end());
				m_failed.clear();
			}
			m_retryRevision = retryRevision;
		}

		void Clear()
		{
			m_entities.clear();
			m_pending.clear();
			m_failed.clear();
			m_bindings.clear();
		}

		std::optional<LocalAabb> Track(Entity entity, const std::string& path, int part)
		{
			if (const auto existing = m_entities.find(entity); existing != m_entities.end())
			{
				const auto& [key, entry] = *existing->second;
				if (key.first == path && key.second == part) return entry.bounds;
				Forget(entity);
			}
			const auto [binding, inserted] = m_bindings.try_emplace(Key{path, part});
			if (inserted) m_pending.insert(&*binding);
			binding->second.entities.insert(entity);
			m_entities.emplace(entity, binding);
			return binding->second.bounds;
		}

		void Forget(Entity entity)
		{
			const auto existing = m_entities.find(entity);
			if (existing == m_entities.end()) return;

			const auto binding = existing->second;
			m_entities.erase(existing);
			binding->second.entities.erase(entity);
			if (!binding->second.entities.empty()) return;

			m_pending.erase(&*binding);
			m_failed.erase(&*binding);
			m_bindings.erase(binding);
		}

		bool HasPending() const { return !m_renderers.empty() && !m_pending.empty(); }

		void Poll(SceneAssetResolver& assets, World& world, VisibilityMatrices& matrices)
		{
			for (auto it = m_pending.begin(); it != m_pending.end();)
			{
				auto* binding = *it;
				const auto& [path, part] = binding->first;
				auto& entry = binding->second;
				bool loading = false;
				bool failed = false;
				bool allBoundsKnown = true;
				std::optional<LocalAabb> bounds;
				for (auto* renderer : m_renderers)
				{
					const auto geometry = assets.ResolveGeometry(path, part, PrimitiveType::Triangle, *renderer);
					loading |= geometry.state == ResourceLoadState::Loading;
					failed |= geometry.state == ResourceLoadState::Failed;
					allBoundsKnown &= geometry.state == ResourceLoadState::Ready && geometry.mesh.localBounds.has_value();
					if (geometry.mesh.localBounds) bounds = geometry.mesh.localBounds;
				}
				if (loading)
				{
					++it;
					continue;
				}

				if (failed) m_failed.insert(binding);
				if (allBoundsKnown && bounds)
				{
					entry.bounds = bounds;
					for (const Entity entity : entry.entities) world.UpdateVisibilityBounds(entity, bounds, matrices);
				}
				it = m_pending.erase(it);
			}
		}

	  private:
		using Key = eastl::pair<std::string, int>;
		struct Entry
		{
			EntitySet entities;
			std::optional<LocalAabb> bounds;
		};
		using Bindings = eastl::map<Key, Entry>;
		using Binding = Bindings::value_type;

		Bindings m_bindings;
		eastl::unordered_map<Entity, Bindings::iterator, std::hash<Entity>> m_entities;
		eastl::unordered_set<Binding*> m_pending;
		eastl::unordered_set<Binding*> m_failed;
		eastl::vector<IRenderAdapter*> m_renderers;
		const SceneAssetResolver* m_assets = nullptr;
		uint64_t m_retryRevision = 0;
	};

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
		    m_registry.on_construct<TransformComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
		    m_registry.on_update<TransformComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
		    m_registry.on_destroy<TransformComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
		    m_registry.on_construct<HierarchyComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
		    m_registry.on_update<HierarchyComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
		    m_registry.on_destroy<HierarchyComponent>().connect<&World::OnVisibilityTransformChanged>(*this),
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
		if (m_visibilityGeometry) m_visibilityGeometry->Clear();
		m_visibilityChanges.clear();
		m_visibilityTransformChanges.clear();
		m_visibilityParents.clear();
		m_visibilityChildren.clear();
		m_rebuildVisibility = true;
	}

	void World::OnVisibilityTransformChanged(entt::registry&, Entity entity)
	{
		m_visibilityTransformChanges.insert(entity);
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

	void World::SyncVisibility(SceneAssetResolver* assets, eastl::span<IRenderAdapter* const> renderers)
	{
		if (assets != nullptr)
		{
			if (!m_visibilityGeometry)
			{
				m_visibilityGeometry = eastl::make_unique<VisibilityGeometry>();
				InvalidateVisibility();
			}
			m_visibilityGeometry->SetSources(*assets, renderers, m_visibilityChanges);
		}
		const bool hasPendingGeometry = assets != nullptr && m_visibilityGeometry->HasPending();
		if (!m_rebuildVisibility && m_visibilityChanges.empty() && m_visibilityTransformChanges.empty() &&
		    !hasPendingGeometry)
			return;

		VisibilityMatrices matrices(m_registry);
		if (m_rebuildVisibility)
		{
			for (const Entity entity : m_registry.view<const HierarchyComponent>()) RefreshVisibilityParent(entity);
			for (const Entity entity : m_registry.view<const MeshRendererComponent, const TransformComponent>())
				SyncEntityVisibility(entity, matrices);
			m_rebuildVisibility = false;
		}
		else
		{
			CollectVisibilityTransformChanges();
			for (const Entity entity : m_visibilityChanges) SyncEntityVisibility(entity, matrices);
		}
		if (!m_visibilityChanges.empty()) m_visibilityChanges.clear();
		if (!m_visibilityTransformChanges.empty()) m_visibilityTransformChanges.clear();
		// Resolve the current bindings after applying ECS changes, including removals and scene restores.
		if (assets != nullptr && m_visibilityGeometry->HasPending())
			m_visibilityGeometry->Poll(*assets, *this, matrices);
	}

	void World::RefreshVisibilityParent(Entity entity)
	{
		const Entity parent = GetParent(m_registry, entity);
		const auto previous = m_visibilityParents.find(entity);
		if (previous != m_visibilityParents.end())
		{
			if (previous->second == parent) return;

			const auto oldChildren = m_visibilityChildren.find(previous->second);
			oldChildren->second.erase(entity);
			if (oldChildren->second.empty()) m_visibilityChildren.erase(oldChildren);
			m_visibilityParents.erase(previous);
		}
		if (parent == NullEntity) return;

		m_visibilityParents.emplace(entity, parent);
		m_visibilityChildren[parent].insert(entity);
	}

	void World::CollectVisibilityTransformChanges()
	{
		if (m_visibilityTransformChanges.empty()) return;

		eastl::vector<Entity> affected(m_visibilityTransformChanges.begin(), m_visibilityTransformChanges.end());
		for (const Entity entity : affected) RefreshVisibilityParent(entity);

		for (size_t index = 0; index < affected.size(); ++index)
		{
			const auto children = m_visibilityChildren.find(affected[index]);
			if (children == m_visibilityChildren.end()) continue;

			for (const Entity child : children->second)
				if (m_visibilityTransformChanges.insert(child).second) affected.push_back(child);
		}
		m_visibilityChanges.insert(affected.begin(), affected.end());
	}

	void World::SyncEntityVisibility(Entity entity, VisibilityMatrices& matrices)
	{
		if (!m_registry.valid(entity))
		{
			if (m_visibilityGeometry) m_visibilityGeometry->Forget(entity);
			(void)m_visibilityRegistry.Remove(entity);
			return;
		}

		const auto* transform = m_registry.try_get<TransformComponent>(entity);
		const auto* mesh = m_registry.try_get<MeshRendererComponent>(entity);
		if (transform == nullptr || mesh == nullptr || !mesh->visible)
		{
			if (m_visibilityGeometry) m_visibilityGeometry->Forget(entity);
			(void)m_visibilityRegistry.Remove(entity);
			return;
		}

		(void)m_visibilityRegistry.Register(entity);
		if (mesh->meshPath.empty())
		{
			if (m_visibilityGeometry) m_visibilityGeometry->Forget(entity);
			UpdateVisibilityBounds(entity, GetPrimitiveLocalBounds(mesh->primitiveType), matrices);
		}
		else
		{
			const auto bounds = m_visibilityGeometry
			                        ? m_visibilityGeometry->Track(entity, mesh->meshPath, mesh->meshPart)
			                        : std::nullopt;
			UpdateVisibilityBounds(entity, bounds, matrices);
		}
	}

	void World::UpdateVisibilityBounds(Entity entity, const std::optional<LocalAabb>& localBounds,
	                                   VisibilityMatrices& matrices)
	{
		const auto worldMatrix = localBounds ? matrices.Get(entity) : std::nullopt;
		if (worldMatrix)
		{
			(void)m_visibilityRegistry.UpdateGeometryBounds(entity, localBounds, *worldMatrix);
		}
		else
		{
			(void)m_visibilityRegistry.UpdateBounds(entity, std::nullopt);
		}
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
