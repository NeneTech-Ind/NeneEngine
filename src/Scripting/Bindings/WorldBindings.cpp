#include "Scripting/Bindings/WorldBindings.h"

#include "Core/NeneLogger.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/World.h"
#include "Scene/Prefab/PrefabLibrary.h"
#include "Scripting/Bindings/ScriptEntity.h"

#include <optional>
#include <sol/sol.hpp>
#include <stdexcept>

namespace NeneEngine
{

	void BindWorldApi(sol::table& nene, ECS::World& world, PrefabLibrary& prefabs)
	{
		auto worldApi = nene.create_named("world");
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
