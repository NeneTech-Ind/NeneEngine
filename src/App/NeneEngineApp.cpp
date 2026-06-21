// NeneEngineApp.cpp

#include "App/NeneEngineApp.h"
#include "App/AppConfig.h"
#include "App/DemoBootstrapRunner.h"
#include "Core/CustomLogger.h"
#include "Core/ExternalLibrarySmokeTest.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Systems/CollisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "Scene/TestScene.h"
#include "Input/KeyCodeStrings.h"
#include "States/PlayState.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace NeneEngine
{
	namespace
	{
		std::string FormatBindings(const std::vector<KeyCode>& keyCodes)
		{
			std::ostringstream stream;
			for (size_t index = 0; index < keyCodes.size(); ++index)
			{
				if (index > 0) stream << ", ";
				stream << ToString(keyCodes[index]);
			}

			return stream.str();
		}

		void LogAppliedInputBindings(const InputManager& inputManager, std::string_view managerName)
		{
			const auto& bindings = inputManager.GetActionBindings();
			NENE_LOG_INFO("Applied input bindings to {}: {} actions", managerName, bindings.size());
			for (const auto& [actionName, keyCodes] : bindings)
			{
				NENE_LOG_INFO("  {} -> [{}]", actionName, FormatBindings(keyCodes));
			}
		}

		void ApplyInputBindings(InputManager& inputManager, const InputConfig& inputConfig, std::string_view managerName)
		{
			inputManager.ClearActionBindings();
			for (const auto& [actionName, keyCodes] : inputConfig.actions)
			{
				inputManager.SetActionBindings(actionName, keyCodes);
			}
			LogAppliedInputBindings(inputManager, managerName);
		}

		std::filesystem::path ResolveLogFilePath()
		{
			auto logRoot = GetExecutableDirectory();
			if (logRoot.empty()) logRoot = std::filesystem::current_path();
			return logRoot / "logs" / "nene_engine.log";
		}
	} // namespace

	NeneEngineApp::NeneEngineApp() = default;

	NeneEngineApp::~NeneEngineApp()
	{
		if (m_running) RequestShutdown();
		m_gameStateMachine.Clear();
		m_windowRuntimeService.Shutdown();
		CustomLogger::GetInstance().Shutdown();
	}

	bool NeneEngineApp::Init(uint32_t width, uint32_t height, const std::string& title)
	{
		try
		{
			// 1. Logger
			CustomLogger::GetInstance().Initialize(ResolveLogFilePath().string(), false, spdlog::level::info,
			                                       true);
			NENE_LOG_INFO("===== NeneEngine v0.4 starting =====");
			ResourceManager::GetInstance().RegisterDefaultLoaders();
			RunExternalLibrarySmokeTests();

			m_runtimeConfigService.LoadStartupConfig();
			const AppConfig& appConfig = m_runtimeConfigService.GetConfig();

			// 2. States
			AppStateContext stateContext{*this, m_world, m_gameStateMachine};
			m_gameStateMachine.PushState(eastl::make_unique<PlayState>(stateContext));

			// 3. ECS
			m_world.AddSystem(std::make_unique<ECS::MovementSystem>());
			m_world.AddSystem(std::make_unique<ECS::CollisionSystem>());
			TestScene::LoadOrCreate(m_world, width, height);
			NENE_LOG_INFO("Test scene loaded from {}", TestScene::DefaultScenePath().string());

			const ECS::Entity primaryCameraEntity = FindPrimaryCameraEntity();
			if (primaryCameraEntity == ECS::NullEntity)
			{
				NENE_LOG_ERROR("Init failed: no primary camera found after loading scene");
				return false;
			}

			if (!m_windowRuntimeService.Initialize(appConfig, m_world, primaryCameraEntity, width, height))
				return false;

			RunDemoBootstrap(m_world, m_windowRuntimeService.GetPrimaryRenderer());

			ApplyRuntimeAppConfig(appConfig);

			NENE_LOG_INFO("Application initialized successfully ({}x{})", width, height);

			return true;
		}
		catch (const std::exception& e)
		{
			NENE_LOG_ERROR("Init failed: {}", e.what());

			return false;
		}
	}

	ECS::Entity NeneEngineApp::FindPrimaryCameraEntity() const
	{
		const auto cameraView = m_world.GetRegistry().view<const ECS::CameraComponent>();
		for (auto entity : cameraView)
		{
			const auto& camera = cameraView.get<ECS::CameraComponent>(entity);
			if (camera.isPrimary) return entity;
		}

		return ECS::NullEntity;
	}

	void NeneEngineApp::ApplyRuntimeAppConfig(const AppConfig& config)
	{
		ApplyInputBindings(m_inputManager, config.input, "app input manager");
		m_windowRuntimeService.ApplyRuntimeConfig(config);
	}

	void NeneEngineApp::CalculateFrameStats()
	{
		// Code computes the average frames per second, and also the
		// average time it takes to render one frame.  These stats
		// are appended to the window caption bar.

		static int frameCnt = 0;
		static float timeElapsed = 0.0f;

		frameCnt++;
		// Compute averages over one-second period.
		if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
		{
			float fps = (float)frameCnt; // fps = frameCnt / 1
			float mspf = 1000.0f / fps;

			std::wstringstream wss;
			wss << std::fixed << std::setprecision(0);
			wss << fps;
			std::wstring fpsStr = wss.str();
			wss.str(L""); // Reset wstringstream
			wss << std::setprecision(6);
			wss << mspf;
			std::wstring mspfStr = wss.str();

			m_windowRuntimeService.UpdateFrameStatsText(fpsStr, mspfStr);

			// Reset for next average.
			frameCnt = 0;
			timeElapsed += 1.0f;
		}
	}

	void NeneEngineApp::LogDeltaTimeStats(float deltaTime)
	{
		static int sampleCount = 0;
		static float accumulatedDeltaTime = 0.0f;
		static float timeElapsed = 0.0f;
		static float lastDeltaTime = 0.0f;

		++sampleCount;
		accumulatedDeltaTime += deltaTime;
		lastDeltaTime = deltaTime;

		if ((m_timer.TotalTime() - timeElapsed) < 1.0f) return;

		const float averageDeltaTime = sampleCount > 0 ? accumulatedDeltaTime / static_cast<float>(sampleCount) : 0.0f;

		NENE_LOG_INFO("deltaTime: last={:.6f} s ({:.3f} ms), avg={:.6f} s ({:.3f} ms), samples={}", lastDeltaTime,
		              lastDeltaTime * 1000.0f, averageDeltaTime, averageDeltaTime * 1000.0f, sampleCount);

		sampleCount = 0;
		accumulatedDeltaTime = 0.0f;
		timeElapsed += 1.0f;
	}

	void NeneEngineApp::PumpWindowMessagesPhase()
	{
		m_windowRuntimeService.PumpWindowMessages();
	}

	void NeneEngineApp::InputPhase(float deltaTime)
	{
		m_inputManager.SetInputDevice(GetFocusedInput());
		m_inputManager.UpdateState();
		m_windowRuntimeService.UpdateInputManagers();
		m_windowRuntimeService.UpdateWindowSystems(deltaTime);
	}

	void NeneEngineApp::GameplayPhase(float deltaTime)
	{
		m_gameStateMachine.HandleInput();
		m_gameStateMachine.Update(deltaTime);
		LogDeltaTimeStats(deltaTime);
		m_runtimeConfigService.Update(deltaTime,
		                              [this](const AppConfig& config) { ApplyRuntimeAppConfig(config); });
	}

	void NeneEngineApp::SyncPhase(float /*deltaTime*/)
	{
		// Reserved explicit phase for synchronizing physics/runtime state back into scene transforms.
	}

	void NeneEngineApp::RenderPhase()
	{
		m_windowRuntimeService.Render();
	}

	void NeneEngineApp::EndFramePhase()
	{
		CalculateFrameStats();
		m_windowRuntimeService.EndFrameInputs();
	}

	void NeneEngineApp::Run()
	{
		m_running = true;
		m_timer.Reset();

		while (m_running && !m_windowRuntimeService.AreAllWindowsClosed())
		{
			PumpWindowMessagesPhase();
			m_timer.Tick();
			const float deltaTime = m_timer.DeltaTime();

			if (!m_isPaused)
			{
				InputPhase(deltaTime);
				GameplayPhase(deltaTime);
				SyncPhase(deltaTime);
				RenderPhase();
				EndFramePhase();
			}
			else
			{
				Sleep(100);
			}
		}
	}

	void NeneEngineApp::RequestShutdown()
	{
		m_running = false;
	}

	InputDevice* NeneEngineApp::GetFocusedInput()
	{
		return m_windowRuntimeService.GetFocusedInput();
	}

	const InputDevice* NeneEngineApp::GetFocusedInput() const
	{
		return m_windowRuntimeService.GetFocusedInput();
	}

} // namespace NeneEngine
