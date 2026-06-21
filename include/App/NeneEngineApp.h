// Application.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppRuntimeConfigService.h"
#include "App/GameStateMachine.h"
#include "Core/Delegate.h"
#include "Core/GameTimer.h"
#include "ECS/Entity.h"
#include "ECS/Systems/ISystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/World.h"
#include "Input/InputDevice.h"
#include "Input/InputManager.h"
#include "Platform/IWindow.h"
#include "RenderAdapters/IRenderAdapter.h"

#include <EASTL/unique_ptr.h>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace NeneEngine
{

	class NeneEngineApp
	{
	  public:
		NeneEngineApp();
		~NeneEngineApp();

		bool Init(uint32_t width = 1280, uint32_t height = 720, const std::string& title = "NeneEngine");
		void Run();
		void RequestShutdown();
		InputDevice* GetFocusedInput();
		const InputDevice* GetFocusedInput() const;
		InputManager& GetInputManager() { return m_inputManager; }
		const InputManager& GetInputManager() const { return m_inputManager; }

	  private:
		// Each OS window owns its rendering path and camera binding; the ECS world is shared.
		struct WindowContext
		{
			eastl::unique_ptr<IWindow> window;
			InputManager inputManager;
			eastl::unique_ptr<IRenderAdapter> renderer;
			std::unique_ptr<ECS::RenderSystem> renderSystem;
			DelegateHandle resizeHandle;
			ECS::Entity cameraEntity = ECS::NullEntity;
			std::string title;
		};

		std::vector<WindowContext> m_windows;
		std::vector<std::unique_ptr<ECS::ISystem>> m_appSystems;

		GameTimer m_timer;
		GameStateMachine m_gameStateMachine;
		ECS::World m_world;
		InputManager m_inputManager;
		AppRuntimeConfigService m_runtimeConfigService;

		std::atomic<bool> m_running{false};
		std::atomic<bool> m_isPaused{false};

		bool CreateWindowContext(uint32_t width, uint32_t height, const std::string& title, ECS::Entity cameraEntity);
		void AddAppSystem(std::unique_ptr<ECS::ISystem> system);
		std::vector<ECS::Entity> CreateAdditionalWindowCameras(size_t count, uint32_t width, uint32_t height);
		ECS::Entity FindPrimaryCameraEntity() const;
		bool AreAllWindowsClosed() const;
		void ApplyRuntimeAppConfig(const AppConfig& config);
		void PumpWindowMessagesPhase();
		void InputPhase(float deltaTime);
		void GameplayPhase(float deltaTime);
		void SyncPhase(float deltaTime);
		void RenderPhase();
		void EndFramePhase();
		void CalculateFrameStats();
		void LogDeltaTimeStats(float deltaTime);
		void HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height);
	};

} // namespace NeneEngine
