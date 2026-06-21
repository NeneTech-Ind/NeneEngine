// AppFrameLoopService.h

#pragma once

#include "App/AppConfig.h"

#include <atomic>
#include <functional>

class GameTimer;

namespace NeneEngine
{
	class AppRuntimeConfigService;
	class AppWindowRuntimeService;
	class GameStateMachine;
	class InputDevice;
	class InputManager;

	class AppFrameLoopService final
	{
	  public:
		using ApplyConfigCallback = std::function<void(const AppConfig&)>;
		using FocusedInputCallback = std::function<InputDevice*()>;

		void Run(std::atomic<bool>& running, const std::atomic<bool>& isPaused, GameTimer& timer,
		         GameStateMachine& gameStateMachine, InputManager& inputManager,
		         AppRuntimeConfigService& runtimeConfigService, AppWindowRuntimeService& windowRuntimeService,
		         ApplyConfigCallback applyRuntimeConfig, FocusedInputCallback getFocusedInput);

	  private:
		void InputPhase(float deltaTime, InputManager& inputManager, AppWindowRuntimeService& windowRuntimeService,
		                FocusedInputCallback getFocusedInput);
		void GameplayPhase(float deltaTime, GameTimer& timer, GameStateMachine& gameStateMachine,
		                   AppRuntimeConfigService& runtimeConfigService, ApplyConfigCallback applyRuntimeConfig);
		void SyncPhase(float deltaTime);
		void EndFramePhase(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService);
		void CalculateFrameStats(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService);
		void LogDeltaTimeStats(GameTimer& timer, float deltaTime);

		int m_frameCount = 0;
		float m_frameStatsTimeElapsed = 0.0f;
		int m_deltaSampleCount = 0;
		float m_accumulatedDeltaTime = 0.0f;
		float m_deltaStatsTimeElapsed = 0.0f;
		float m_lastDeltaTime = 0.0f;
	};

} // namespace NeneEngine
