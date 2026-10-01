// AppBootstrapService.cpp

#include "App/AppBootstrapService.h"

#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "App/DemoBootstrapRunner.h"
#include "App/GameStateMachine.h"
#include "App/NeneEngineApp.h"
#include "Core/CustomLogger.h"
#include "Core/ExternalLibrarySmokeTest.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Events/CollisionEvent.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "ECS/Systems/PlayerControllerSystem.h"
#include "Scene/DemoScene.h"
#include "GameStates/PlayState.h"

namespace NeneEngine
{
	namespace
	{
		ECS::Entity FindPrimaryCameraEntity(const ECS::World& world)
		{
			const auto cameraView = world.GetRegistry().view<const ECS::CameraComponent>();
			for (auto entity : cameraView)
			{
				const auto& camera = cameraView.get<ECS::CameraComponent>(entity);
				if (camera.isPrimary) return entity;
			}

			return ECS::NullEntity;
		}

		std::string DescribeEntity(const ECS::World& world, ECS::Entity entity)
		{
			const auto* tag = world.GetRegistry().try_get<ECS::TagComponent>(entity);
			if (tag != nullptr && !tag->name.empty()) return tag->name;
			return "Entity#" + std::to_string(entt::to_integral(entity));
		}

		void SubscribeCollisionLogger(ECS::World& world)
		{
			world.GetEventBus().Subscribe<ECS::CollisionEvent>(
			    [&world](const ECS::CollisionEvent& event)
			    {
				    NENE_LOG_INFO(
				        "Collision: '{}' <-> '{}' at ({:.2f}, {:.2f}, {:.2f}), normal ({:.2f}, {:.2f}, {:.2f})",
				        DescribeEntity(world, event.entityA), DescribeEntity(world, event.entityB),
				        event.contactPoint.x, event.contactPoint.y, event.contactPoint.z, event.normal.x,
				        event.normal.y, event.normal.z);
			    });
		}
	} // namespace

	bool AppBootstrapService::Initialize(NeneEngineApp& app, GameStateMachine& gameStateMachine, ECS::World& world,
	                                     AppRuntimeConfigService& runtimeConfigService,
	                                     AppWindowRuntimeService& windowRuntimeService,
	                                     ApplyConfigCallback applyRuntimeConfig, const std::string& logFilePath,
	                                     uint32_t width, uint32_t height)
	{
		CustomLogger::GetInstance().Initialize(logFilePath, false, spdlog::level::info, true);
		NENE_LOG_INFO("===== NeneEngine v0.4 starting =====");
		ResourceManager::GetInstance().RegisterDefaultLoaders();
		RunExternalLibrarySmokeTests();

		runtimeConfigService.LoadStartupConfig();
		const AppConfig& appConfig = runtimeConfigService.GetConfig();

		AppStateContext stateContext{app, world, gameStateMachine};
		gameStateMachine.PushState(eastl::make_unique<PlayState>(stateContext));

		world.AddSystem(std::make_unique<ECS::MovementSystem>());
		world.AddSystem(std::make_unique<ECS::PlayerControllerSystem>(app.GetInputManager()));
		world.AddSystem(std::make_unique<ECS::PhysicsSystem>());
		SubscribeCollisionLogger(world);
		DemoScene::LoadOrCreate(world, width, height);
		NENE_LOG_INFO("Demo scene loaded from {}", DemoScene::DefaultScenePath().string());

		const ECS::Entity primaryCameraEntity = FindPrimaryCameraEntity(world);
		if (primaryCameraEntity == ECS::NullEntity)
		{
			NENE_LOG_ERROR("Init failed: no primary camera found after loading scene");
			return false;
		}

		if (!windowRuntimeService.Initialize(appConfig, world, primaryCameraEntity, width, height)) return false;

		RunDemoBootstrap(world, windowRuntimeService.GetRenderers());
		applyRuntimeConfig(appConfig);

		NENE_LOG_INFO("Application initialized successfully ({}x{})", width, height);
		return true;
	}

} // namespace NeneEngine
