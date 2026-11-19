#include "ECS/Systems/ScriptSystem.h"

#include "ECS/Components/ScriptComponent.h"
#include "ECS/Events/CollisionEvent.h"
#include "ECS/World.h"
#include "Scripting/Bindings/ScriptEntity.h"
#include "Scripting/LuaBehaviour.h"
#include "Scripting/ScriptEngine.h"

#include <EASTL/unordered_map.h>
#include <cstdint>
#include <filesystem>
#include <sol/sol.hpp>
#include <string>
#include <utility>

namespace NeneEngine::ECS
{
	struct ScriptSystem::Impl
	{
		struct RuntimeBehaviour
		{
			std::string scriptPath;
			std::uint64_t scriptRevision;
			lua_State* luaState = nullptr;
			eastl::unique_ptr<LuaBehaviour> behaviour;
		};

		ScriptEngine& scriptEngine;
		World& world;
		eastl::unordered_map<Entity, RuntimeBehaviour> behaviours;
		eastl::unordered_map<Entity, std::string> failedScripts;
		DelegateHandle collisionSubscription;

		Impl(ScriptEngine& engine, World& ecsWorld) : scriptEngine(engine), world(ecsWorld)
		{
			collisionSubscription = world.GetEventBus().Subscribe<CollisionEvent>(
			    [this](const CollisionEvent& event) { OnCollision(event); });
		}

		~Impl()
		{
			world.GetEventBus().Unsubscribe(collisionSubscription);
			ClearBehaviours();
		}

		eastl::unique_ptr<LuaBehaviour> CreateBehaviour(Entity entity, const ScriptComponent& component,
		                                               const sol::table& scriptClass, const ScriptFieldSchema& fieldSchema)
		{
			sol::state_view lua{scriptClass.lua_state()};
			auto entityApi = sol::make_object(lua, ScriptEntity(world, entity));
			return eastl::make_unique<LuaBehaviour>(scriptClass, component.scriptPath, fieldSchema, component.fields,
			                                       entityApi);
		}

		void Update(float deltaTime)
		{
			auto& registry = world.GetRegistry();
			auto view = registry.view<ScriptComponent>();
			for (const Entity entity : view)
			{
				const ScriptComponent& component = view.get<ScriptComponent>(entity);
				const std::filesystem::path scriptPath{component.scriptPath};
				const auto revision = component.scriptPath.empty() ? 0 : scriptEngine.GetScriptRevision(scriptPath);
				auto runtime = behaviours.find(entity);
				if (runtime != behaviours.end() && runtime->second.scriptPath != component.scriptPath)
				{
					DestroyBehaviour(entity);
					runtime = behaviours.end();
				}

				if (component.scriptPath.empty())
				{
					failedScripts.erase(entity);
					continue;
				}

				if (runtime == behaviours.end())
				{
					const auto failed = failedScripts.find(entity);
					if (failed != failedScripts.end() && failed->second == component.scriptPath && revision == 0)
						continue;
					failedScripts.erase(entity);

					const sol::table* scriptClass = scriptEngine.LoadScript(scriptPath);
					const ScriptFieldSchema* fieldSchema = scriptEngine.GetFieldSchema(scriptPath);
					if (scriptClass == nullptr || fieldSchema == nullptr)
					{
						failedScripts.emplace(entity, component.scriptPath);
						continue;
					}

					RuntimeBehaviour newRuntime{component.scriptPath, scriptEngine.GetScriptRevision(scriptPath),
					                            scriptClass->lua_state(),
					                            CreateBehaviour(entity, component, *scriptClass, *fieldSchema)};
					runtime = behaviours.emplace(entity, std::move(newRuntime)).first;
					failedScripts.erase(entity);
					runtime->second.behaviour->OnStart();
				}
				else if (runtime->second.scriptRevision != revision)
				{
					const auto* scriptClass = scriptEngine.LoadScript(scriptPath);
					const auto* fieldSchema = scriptEngine.GetFieldSchema(scriptPath);
					if (scriptClass && fieldSchema)
					{
						auto& behaviour = runtime->second.behaviour;
						if (behaviour->HasStarted())
							behaviour->Reload(*scriptClass, *fieldSchema, component.fields);
						else
						{
							behaviour->OnDestroy();
							behaviour = CreateBehaviour(entity, component, *scriptClass, *fieldSchema);
							behaviour->OnStart();
						}
						runtime->second.scriptRevision = revision;
					}
				}

				runtime->second.behaviour->OnUpdate(deltaTime);
			}

			for (auto runtime = behaviours.begin(); runtime != behaviours.end();)
			{
				const Entity entity = runtime->first;
				const auto* component = registry.try_get<ScriptComponent>(entity);
				if (registry.valid(entity) && component != nullptr &&
				    component->scriptPath == runtime->second.scriptPath)
				{
					++runtime;
					continue;
				}

				if (runtime->second.behaviour != nullptr) runtime->second.behaviour->OnDestroy();
				runtime = behaviours.erase(runtime);
			}

			for (auto failed = failedScripts.begin(); failed != failedScripts.end();)
			{
				const auto* component = registry.try_get<ScriptComponent>(failed->first);
				if (registry.valid(failed->first) && component != nullptr && component->scriptPath == failed->second)
					++failed;
				else
					failed = failedScripts.erase(failed);
			}
		}

		void OnCollision(const CollisionEvent& event)
		{
			if (event.entityA == event.entityB) return;

			auto first = behaviours.find(event.entityA);
			if (first != behaviours.end() && world.GetRegistry().valid(event.entityA))
			{
				sol::state_view lua{first->second.luaState};
				sol::object other = sol::make_object(lua, ScriptEntity(world, event.entityB));
				first->second.behaviour->OnCollision(other, event.normal);
			}

			auto second = behaviours.find(event.entityB);
			if (second != behaviours.end() && world.GetRegistry().valid(event.entityB))
			{
				sol::state_view lua{second->second.luaState};
				sol::object other = sol::make_object(lua, ScriptEntity(world, event.entityA));
				second->second.behaviour->OnCollision(other, -event.normal);
			}
		}

		void DestroyBehaviour(Entity entity)
		{
			const auto runtime = behaviours.find(entity);
			if (runtime == behaviours.end()) return;

			if (runtime->second.behaviour != nullptr) runtime->second.behaviour->OnDestroy();
			behaviours.erase(runtime);
			failedScripts.erase(entity);
		}

		void ClearBehaviours()
		{
			for (auto& entry : behaviours)
			{
				auto& runtime = entry.second;
				if (runtime.behaviour != nullptr) runtime.behaviour->OnDestroy();
			}
			behaviours.clear();
			failedScripts.clear();
		}
	};

	ScriptSystem::ScriptSystem(ScriptEngine& scriptEngine, World& world)
	    : m_impl(eastl::make_unique<Impl>(scriptEngine, world))
	{
	}

	ScriptSystem::~ScriptSystem() = default;

	void ScriptSystem::Update(World& world, float deltaTime)
	{
		(void)world;
		m_impl->Update(deltaTime);
	}

	void ScriptSystem::OnSceneRestored(World& world)
	{
		(void)world;
		m_impl->ClearBehaviours();
	}

	void ScriptSystem::OnPlayStopping(World& world)
	{
		(void)world;
		m_impl->ClearBehaviours();
	}

	void ScriptSystem::OnEntityDestroying(World& world, Entity entity)
	{
		(void)world;
		m_impl->DestroyBehaviour(entity);
	}

} // namespace NeneEngine::ECS
