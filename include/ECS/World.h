// World.h

#pragma once

#include "EventBus.h"
#include "Entity.h"
#include "Systems/ISystem.h"

#include <EASTL/deque.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <cstdint>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <utility>

namespace NeneEngine::ECS
{

	class World;

	struct WorldLifetime
	{
		World* world = nullptr;
		uint64_t sceneGeneration = 0;
	};

	class World
	{
	  public:
		World();
		~World();

		World(const World&) = delete;
		World& operator=(const World&) = delete;
		World(World&&) = delete;
		World& operator=(World&&) = delete;

		// ===== Entity =====
		Entity CreateEntity(const std::string& name = "Entity");
		void DestroyEntity(Entity entity);
		void DeferDestroyEntity(Entity entity);

		// ===== Components =====
		template <typename Component, typename... Args> Component& AddComponent(Entity entity, Args&&... args)
		{
			return m_registry.emplace<Component>(entity, std::forward<Args>(args)...);
		}

		template <typename Component> Component* GetComponent(Entity entity)
		{
			return m_registry.try_get<Component>(entity);
		}

		template <typename Component> bool HasComponent(Entity entity) const
		{
			return m_registry.any_of<Component>(entity);
		}

		template <typename Component> void RemoveComponent(Entity entity) { m_registry.remove<Component>(entity); }

		// ===== Systems =====
		void AddSystem(eastl::unique_ptr<ISystem> system);

		// ===== Update =====
		void Update(float deltaTime);

		// ===== Play =====
		void NotifyPlayStarted();
		void NotifyPlayStopping();
		void RequestPlayStop() { m_playStopRequested = true; }
		[[nodiscard]] bool ConsumePlayStopRequest() { return std::exchange(m_playStopRequested, false); }

		EventBus& GetEventBus() { return m_eventBus; }
		const EventBus& GetEventBus() const { return m_eventBus; }

		entt::registry& GetRegistry() { return m_registry; }
		const entt::registry& GetRegistry() const { return m_registry; }
		[[nodiscard]] std::weak_ptr<const WorldLifetime> GetLifetime() const { return m_lifetime; }

	  private:
		void DetachFromParent(Entity entity);
		void FlushDeferredDestruction();

		EventBus m_eventBus;
		entt::registry m_registry;
		eastl::vector<eastl::unique_ptr<ISystem>> m_systems;
		eastl::deque<Entity> m_pendingDestruction;
		std::shared_ptr<WorldLifetime> m_lifetime;
		bool m_playStopRequested = false;
	};

} // namespace NeneEngine::ECS
