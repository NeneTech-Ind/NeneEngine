// World.h

#pragma once

#include "EventBus.h"
#include "Entity.h"
#include "Systems/ISystem.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <entt/entt.hpp>
#include <string>
#include <utility>

namespace NeneEngine::ECS
{

	class World
	{
	  public:
		World();
		~World() = default;

		// ===== Entity =====
		Entity CreateEntity(const std::string& name = "Entity");
		void DestroyEntity(Entity entity);

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

		// ===== Update / Render =====
		void Update(float deltaTime);
		void Render();

		EventBus& GetEventBus() { return m_eventBus; }
		const EventBus& GetEventBus() const { return m_eventBus; }

		entt::registry& GetRegistry() { return m_registry; }
		const entt::registry& GetRegistry() const { return m_registry; }

	  private:
		EventBus m_eventBus;
		entt::registry m_registry;
		eastl::vector<eastl::unique_ptr<ISystem>> m_systems;
	};

} // namespace NeneEngine::ECS
