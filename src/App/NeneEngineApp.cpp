// NeneEngineApp.cpp

#include "App/NeneEngineApp.h"
#include "App/AppConfig.h"
#include "App/DemoBootstrapRunner.h"
#include "Core/CustomLogger.h"
#include "Core/ExternalLibrarySmokeTest.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Systems/CameraControllerSystem.h"
#include "ECS/Systems/CollisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/PrimitiveControlSystem.h"
#include "Platform/Windows32/Windows32Window.h"
#include "RenderAdapters/DiligentDX12Adapter.h"
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

		for (auto& windowContext : m_windows)
		{
			if (windowContext.window && windowContext.resizeHandle.IsValid())
			{
				windowContext.window->OnResized().Remove(windowContext.resizeHandle);
				windowContext.resizeHandle.Reset();
			}
		}

		m_windows.clear();
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

			if (appConfig.windows.empty())
			{
				NENE_LOG_ERROR("Init failed: app config does not contain any windows");
				return false;
			}

			size_t mainWindowCount = 0;
			size_t secondaryWindowCount = 0;
			for (const auto& windowConfig : appConfig.windows)
			{
				if (windowConfig.isMain)
					++mainWindowCount;
				else
					++secondaryWindowCount;
			}

			if (mainWindowCount == 0)
			{
				NENE_LOG_ERROR("Init failed: no main window defined in config");
				return false;
			}

			if (mainWindowCount > 1)
				NENE_LOG_WARN(
				    "App config: multiple windows marked as main, only the first one will control the primary camera");

			m_windows.reserve(appConfig.windows.size());

			const auto secondaryCameraEntities = CreateAdditionalWindowCameras(secondaryWindowCount, width, height);
			size_t secondaryCameraIndex = 0;
			bool mainWindowCreated = false;

			for (const auto& windowConfig : appConfig.windows)
			{
				ECS::Entity cameraEntity = primaryCameraEntity;
				if (windowConfig.isMain && !mainWindowCreated)
				{
					mainWindowCreated = true;
				}
				else
				{
					if (secondaryCameraIndex >= secondaryCameraEntities.size())
					{
						NENE_LOG_ERROR("Init failed: not enough secondary cameras for configured windows");
						return false;
					}

					cameraEntity = secondaryCameraEntities[secondaryCameraIndex++];
				}

				if (!CreateWindowContext(windowConfig.width, windowConfig.height, windowConfig.title, cameraEntity))
					return false;
			}

			RunDemoBootstrap(m_world, m_windows.empty() ? nullptr : m_windows.front().renderer.get());

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

	bool NeneEngineApp::CreateWindowContext(uint32_t width, uint32_t height, const std::string& title,
	                                        ECS::Entity cameraEntity)
	{
		WindowContext windowContext{};
		windowContext.title = title;
		windowContext.cameraEntity = cameraEntity;
		windowContext.window = eastl::make_unique<Windows32Window>();
		if (!windowContext.window->Create(width, height, title))
		{
			NENE_LOG_ERROR("Failed to create window '{}'", title);
			return false;
		}

		windowContext.renderer = eastl::make_unique<DiligentDX12Adapter>();
		if (!windowContext.renderer->Init(windowContext.window->GetHWND(), width, height))
		{
			NENE_LOG_ERROR("Failed to initialize renderer for window '{}'", title);
			return false;
		}

		const size_t windowIndex = m_windows.size();
		windowContext.resizeHandle =
		    windowContext.window->OnResized().AddLambda([this, windowIndex](uint32_t newWidth, uint32_t newHeight)
		                                                { HandleWindowResize(windowIndex, newWidth, newHeight); });

		m_windows.push_back(std::move(windowContext));
		auto& storedWindowContext = m_windows.back();
		storedWindowContext.inputManager.SetInputDevice(&storedWindowContext.window->GetInput());
		storedWindowContext.renderSystem =
		    std::make_unique<ECS::RenderSystem>(storedWindowContext.renderer.get(), cameraEntity);
		AddAppSystem(std::make_unique<ECS::CameraControllerSystem>(storedWindowContext.inputManager, cameraEntity));
		AddAppSystem(std::make_unique<ECS::PrimitiveControlSystem>(storedWindowContext.inputManager));

		HandleWindowResize(windowIndex, width, height);
		return true;
	}

	void NeneEngineApp::AddAppSystem(std::unique_ptr<ECS::ISystem> system)
	{
		m_appSystems.push_back(std::move(system));
	}

	std::vector<ECS::Entity> NeneEngineApp::CreateAdditionalWindowCameras(size_t count, uint32_t width, uint32_t height)
	{
		std::vector<ECS::Entity> secondaryCameraEntities;
		secondaryCameraEntities.reserve(count);

		const ECS::Entity primaryCameraEntity = FindPrimaryCameraEntity();
		if (primaryCameraEntity == ECS::NullEntity) return secondaryCameraEntities;

		const auto* primaryTransform = m_world.GetComponent<ECS::TransformComponent>(primaryCameraEntity);
		const auto* primaryCamera = m_world.GetComponent<ECS::CameraComponent>(primaryCameraEntity);
		const auto* primaryController = m_world.GetComponent<ECS::CameraControllerComponent>(primaryCameraEntity);
		if (primaryTransform == nullptr || primaryCamera == nullptr || primaryController == nullptr)
			return secondaryCameraEntities;

		for (size_t cameraIndex = 0; cameraIndex < count; ++cameraIndex)
		{
			const ECS::Entity secondaryCameraEntity =
			    m_world.CreateEntity("SecondaryCamera" + std::to_string(cameraIndex + 1));
			auto& secondaryTransform = m_world.AddComponent<ECS::TransformComponent>(secondaryCameraEntity);
			secondaryTransform = *primaryTransform;
			secondaryTransform.position.x += 2.0f * static_cast<float>(cameraIndex + 1);

			auto& secondaryCamera = m_world.AddComponent<ECS::CameraComponent>(secondaryCameraEntity);
			secondaryCamera = *primaryCamera;
			secondaryCamera.aspectRatio = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
			secondaryCamera.isPrimary = false;

			auto& secondaryController = m_world.AddComponent<ECS::CameraControllerComponent>(secondaryCameraEntity);
			secondaryController = *primaryController;

			secondaryCameraEntities.push_back(secondaryCameraEntity);
		}

		return secondaryCameraEntities;
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

	bool NeneEngineApp::AreAllWindowsClosed() const
	{
		if (m_windows.empty()) return true;

		for (const auto& windowContext : m_windows)
		{
			if (windowContext.window && !windowContext.window->ShouldClose()) return false;
		}

		return true;
	}

	void NeneEngineApp::ApplyRuntimeAppConfig(const AppConfig& config)
	{
		ApplyInputBindings(m_inputManager, config.input, "app input manager");

		for (size_t index = 0; index < m_windows.size(); ++index)
		{
			auto& windowContext = m_windows[index];
			const std::string managerName = "window input manager[" + std::to_string(index) + "]";
			ApplyInputBindings(windowContext.inputManager, config.input, managerName);
			if (windowContext.renderer) windowContext.renderer->SetClearColor(config.window.backgroundColor);
		}
	}

	void NeneEngineApp::HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height)
	{
		if (windowIndex >= m_windows.size()) return;

		if (width == 0 || height == 0)
		{
			NENE_LOG_WARN("Application resize ignored for window {} with invalid size {}x{}", windowIndex, width,
			              height);
			return;
		}

		auto& windowContext = m_windows[windowIndex];
		if (windowContext.renderer) windowContext.renderer->Resize(width, height);

		if (auto* camera = m_world.GetComponent<ECS::CameraComponent>(windowContext.cameraEntity))
			camera->aspectRatio = static_cast<float>(width) / static_cast<float>(height);

		NENE_LOG_INFO("Window '{}' resized to {}x{}", windowContext.title, width, height);
	}

	inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
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

			for (const auto& windowContext : m_windows)
			{
				if (!windowContext.window || windowContext.window->ShouldClose()) continue;

				std::wstring windowText =
				    AnsiToWString(windowContext.window->GetTitle()) + L" | fps: " + fpsStr + L" | mspf: " + mspfStr;

				SetWindowTextW(windowContext.window->GetHWND(), windowText.c_str());
			}

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
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window && !windowContext.window->ShouldClose()) windowContext.window->PumpMessages();
		}
	}

	void NeneEngineApp::InputPhase(float deltaTime)
	{
		m_inputManager.SetInputDevice(GetFocusedInput());
		m_inputManager.UpdateState();

		for (auto& windowContext : m_windows)
		{
			if (windowContext.window) windowContext.inputManager.UpdateState();
		}

		// Window-bound input systems stay app-owned so World contains gameplay logic only.
		for (auto& system : m_appSystems) system->Update(m_world, deltaTime);
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
		for (auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;
			if (!windowContext.renderer || !windowContext.renderSystem) continue;

			// Render systems are per-window so the same world can be drawn from different cameras.
			windowContext.renderer->BeginFrame();
			windowContext.renderSystem->Render(m_world);
			windowContext.renderer->EndFrame();
			windowContext.renderer->Present();
		}
	}

	void NeneEngineApp::EndFramePhase()
	{
		CalculateFrameStats();
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window) windowContext.window->GetInput().EndFrame();
		}
	}

	void NeneEngineApp::Run()
	{
		m_running = true;
		m_timer.Reset();

		while (m_running && !AreAllWindowsClosed())
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
		for (auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;

			InputDevice& input = windowContext.window->GetInput();
			if (input.IsFocused()) return &input;
		}

		return nullptr;
	}

	const InputDevice* NeneEngineApp::GetFocusedInput() const
	{
		for (const auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;

			const InputDevice& input = windowContext.window->GetInput();
			if (input.IsFocused()) return &input;
		}

		return nullptr;
	}

} // namespace NeneEngine
