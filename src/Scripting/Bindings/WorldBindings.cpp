#include "Scripting/Bindings/WorldBindings.h"

#include "Core/NeneLogger.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Scene/Prefab/PrefabLibrary.h"
#include "Scripting/Bindings/ScriptEntity.h"

#include <EASTL/unordered_set.h>
#include <cmath>
#include <optional>
#include <sol/sol.hpp>
#include <stdexcept>

namespace NeneEngine
{
	namespace
	{
		std::optional<glm::dvec3> FindWorldPosition(const entt::registry& registry, ECS::Entity entity,
		                                           const ECS::TransformComponent& transform)
		{
			glm::dvec4 position(glm::dvec3(transform.position), 1.0);
			eastl::unordered_set<ECS::Entity> visited;
			while (const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity))
			{
				entity = hierarchy->parent;
				if (!registry.valid(entity)) break;
				if (!visited.insert(entity).second) return std::nullopt;
				if (const auto* parent = registry.try_get<ECS::TransformComponent>(entity))
					position = glm::dmat4(parent->GetModelMatrix()) * position;
			}
			return glm::dvec3(position);
		}
	}

	void BindWorldApi(sol::table& nene, ECS::World& world, PrefabLibrary& prefabs)
	{
		auto worldApi = nene.create_named("world");
		worldApi.set_function("stop_play", sol::protect(
		                                      [lifetime = world.GetLifetime()]
		                                      {
			                                      const auto live = lifetime.lock();
			                                      if (!live || !live->world)
				                                      throw std::runtime_error("World is no longer valid");
			                                      live->world->RequestPlayStop();
		                                      }));
		worldApi.set_function(
		    "find", sol::protect(
		                [lifetime = world.GetLifetime()](const std::string& name) -> std::optional<ScriptEntity>
		                {
			                const auto live = lifetime.lock();
			                if (!live || !live->world) throw std::runtime_error("World is no longer valid");
			                ECS::Entity found = ECS::NullEntity;
			                const auto& registry = live->world->GetRegistry();
			                for (const auto [entity, tag] : registry.view<const ECS::TagComponent>().each())
				                if (tag.name == name &&
				                    (found == ECS::NullEntity || entt::to_integral(entity) < entt::to_integral(found)))
					                found = entity;
			                if (found == ECS::NullEntity) return std::nullopt;
			                return ScriptEntity(*live->world, found);
		                }));
		worldApi.set_function("find_by_tag",
		                      sol::protect(
		                          [lifetime = world.GetLifetime()](sol::this_state state, const std::string& category)
		                          {
			                          const auto live = lifetime.lock();
			                          if (!live || !live->world) throw std::runtime_error("World is no longer valid");
			                          auto& currentWorld = *live->world;
			                          const auto& registry = currentWorld.GetRegistry();
			                          auto matches = sol::state_view(state).create_table();
			                          int index = 1;
			                          for (const auto [entity, tag] : registry.view<const ECS::TagComponent>().each())
			                          {
				                          if (tag.tags.contains(category))
					                          matches.raw_set(index++, ScriptEntity(currentWorld, entity));
			                          }
			                          return matches;
		                          }));
		const auto findInRadius = [lifetime = world.GetLifetime()](sol::this_state state, const glm::vec3& center,
		                                                         double radius)
		{
			if (!std::isfinite(radius) || radius < 0.0)
				throw std::invalid_argument("Search radius must be finite and non-negative");
			if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
				throw std::invalid_argument("Search center must be finite");
			const auto live = lifetime.lock();
			if (!live || !live->world) throw std::runtime_error("World is no longer valid");

			auto& currentWorld = *live->world;
			const auto& registry = currentWorld.GetRegistry();
			const glm::dvec3 origin(center);
			auto matches = sol::state_view(state).create_table();
			int index = 1;
			for (const auto [entity, transform] : registry.view<const ECS::TransformComponent>().each())
			{
				const auto position = FindWorldPosition(registry, entity, transform);
				if (!position) continue;
				const auto offset = *position - origin;
				if (std::hypot(offset.x, offset.y, offset.z) <= radius)
					matches.raw_set(index++, ScriptEntity(currentWorld, entity));
			}
			return matches;
		};
		worldApi.set_function("find_in_radius", sol::protect(findInRadius));
		worldApi.set_function("spawn",
		                      sol::protect(
		                          [lifetime = world.GetLifetime(), &prefabs](
		                              const std::string& name, const glm::vec3& position) -> std::optional<ScriptEntity>
		                          {
			                          const auto live = lifetime.lock();
			                          if (!live || !live->world) throw std::runtime_error("World is no longer valid");
			                          try
			                          {
				                          return ScriptEntity(*live->world, prefabs.Instantiate(name, position));
			                          }
			                          catch (const std::exception& error)
			                          {
				                          NENE_LOG_ERROR("Cannot spawn prefab '{}': {}", name, error.what());
				                          return std::nullopt;
			                          }
		                          }));
		worldApi.set_function("destroy", sol::protect([](const ScriptEntity& entity)
		                                              { entity.GetWorld().DeferDestroyEntity(entity.GetId()); }));
	}

}
