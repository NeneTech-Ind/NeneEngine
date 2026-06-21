// Application.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppBootstrapService.h"
#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "App/GameStateMachine.h"
#include "Core/GameTimer.h"
#include "ECS/World.h"
#include "Input/InputDevice.h"
#include "Input/InputManager.h"

#include <atomic>
#include <string>

namespace NeneEngine
{

	class NeneEngineApp
	{
		friend class AppBootstrapService;

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
		GameTimer m_timer;
		GameStateMachine m_gameStateMachine;
		ECS::World m_world;
		InputManager m_inputManager;
		AppBootstrapService m_bootstrapService;
		AppRuntimeConfigService m_runtimeConfigService;
		AppWindowRuntimeService m_windowRuntimeService;

		std::atomic<bool> m_running{false};
		std::atomic<bool> m_isPaused{false};

		void ApplyRuntimeAppConfig(const AppConfig& config);
		void PumpWindowMessagesPhase();
		void InputPhase(float deltaTime);
		void GameplayPhase(float deltaTime);
		void SyncPhase(float deltaTime);
		void RenderPhase();
		void EndFramePhase();
		void CalculateFrameStats();
		void LogDeltaTimeStats(float deltaTime);
	};

} // namespace NeneEngine
