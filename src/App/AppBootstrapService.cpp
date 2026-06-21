// AppBootstrapService.cpp

#include "App/AppBootstrapService.h"

#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "App/DemoBootstrapRunner.h"
#include "App/GameStateMachine.h"
#include "Core/CustomLogger.h"
#include "Core/ExternalLibrarySmokeTest.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Systems/CollisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "Scene/TestScene.h"
#include "States/PlayState.h"

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
		world.AddSystem(std::make_unique<ECS::CollisionSystem>());
		TestScene::LoadOrCreate(world, width, height);
		NENE_LOG_INFO("Test scene loaded from {}", TestScene::DefaultScenePath().string());

		const ECS::Entity primaryCameraEntity = FindPrimaryCameraEntity(world);
		if (primaryCameraEntity == ECS::NullEntity)
		{
			NENE_LOG_ERROR("Init failed: no primary camera found after loading scene");
			return false;
		}

		if (!windowRuntimeService.Initialize(appConfig, world, primaryCameraEntity, width, height)) return false;

		RunDemoBootstrap(world, windowRuntimeService.GetPrimaryRenderer());
		applyRuntimeConfig(appConfig);

		NENE_LOG_INFO("Application initialized successfully ({}x{})", width, height);
		return true;
	}

} // namespace NeneEngine
