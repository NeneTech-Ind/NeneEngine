// AppFrameLoopService.cpp

#include "App/AppFrameLoopService.h"

#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "App/GameStateMachine.h"
#include "Core/CustomLogger.h"
#include "Core/GameTimer.h"
#include "Input/InputDevice.h"
#include "Input/InputManager.h"

#include <Windows.h>
#include <iomanip>
#include <sstream>

namespace NeneEngine
{
	void AppFrameLoopService::Run(std::atomic<bool>& running, const std::atomic<bool>& isPaused, GameTimer& timer,
	                              GameStateMachine& gameStateMachine, InputManager& inputManager,
	                              AppRuntimeConfigService& runtimeConfigService,
	                              AppWindowRuntimeService& windowRuntimeService,
	                              ApplyConfigCallback applyRuntimeConfig, FocusedInputCallback getFocusedInput)
	{
		running = true;
		timer.Reset();

		while (running.load() && !windowRuntimeService.AreAllWindowsClosed())
		{
			windowRuntimeService.PumpWindowMessages();
			timer.Tick();
			const float deltaTime = timer.DeltaTime();

			if (!isPaused.load())
			{
				InputPhase(deltaTime, inputManager, windowRuntimeService, getFocusedInput);
				GameplayPhase(deltaTime, timer, gameStateMachine, runtimeConfigService, applyRuntimeConfig);
				SyncPhase(deltaTime);
				windowRuntimeService.Render();
				EndFramePhase(timer, windowRuntimeService);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	void AppFrameLoopService::InputPhase(float deltaTime, InputManager& inputManager,
	                                     AppWindowRuntimeService& windowRuntimeService,
	                                     FocusedInputCallback getFocusedInput)
	{
		inputManager.SetInputDevice(getFocusedInput());
		inputManager.UpdateState();
		windowRuntimeService.UpdateInputManagers();
		windowRuntimeService.UpdateWindowSystems(deltaTime);
	}

	void AppFrameLoopService::GameplayPhase(float deltaTime, GameTimer& timer, GameStateMachine& gameStateMachine,
	                                        AppRuntimeConfigService& runtimeConfigService,
	                                        ApplyConfigCallback applyRuntimeConfig)
	{
		gameStateMachine.HandleInput();
		gameStateMachine.Update(deltaTime);
		LogDeltaTimeStats(timer, deltaTime);
		runtimeConfigService.Update(deltaTime, applyRuntimeConfig);
	}

	void AppFrameLoopService::SyncPhase(float /*deltaTime*/)
	{
		// Reserved explicit phase for synchronizing physics/runtime state back into scene transforms.
	}

	void AppFrameLoopService::EndFramePhase(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService)
	{
		CalculateFrameStats(timer, windowRuntimeService);
		windowRuntimeService.EndFrameInputs();
	}

	void AppFrameLoopService::CalculateFrameStats(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService)
	{
		++m_frameCount;
		if ((timer.TotalTime() - m_frameStatsTimeElapsed) < 1.0f) return;

		const float fps = static_cast<float>(m_frameCount);
		const float mspf = 1000.0f / fps;

		std::wstringstream wss;
		wss << std::fixed << std::setprecision(0) << fps;
		const std::wstring fpsStr = wss.str();
		wss.str(L"");
		wss.clear();
		wss << std::setprecision(6) << mspf;
		const std::wstring mspfStr = wss.str();

		windowRuntimeService.UpdateFrameStatsText(fpsStr, mspfStr);

		m_frameCount = 0;
		m_frameStatsTimeElapsed += 1.0f;
	}

	void AppFrameLoopService::LogDeltaTimeStats(GameTimer& timer, float deltaTime)
	{
		++m_deltaSampleCount;
		m_accumulatedDeltaTime += deltaTime;
		m_lastDeltaTime = deltaTime;

		if ((timer.TotalTime() - m_deltaStatsTimeElapsed) < 1.0f) return;

		const float averageDeltaTime =
		    m_deltaSampleCount > 0 ? m_accumulatedDeltaTime / static_cast<float>(m_deltaSampleCount) : 0.0f;

		NENE_LOG_INFO("deltaTime: last={:.6f} s ({:.3f} ms), avg={:.6f} s ({:.3f} ms), samples={}", m_lastDeltaTime,
		              m_lastDeltaTime * 1000.0f, averageDeltaTime, averageDeltaTime * 1000.0f, m_deltaSampleCount);

		m_deltaSampleCount = 0;
		m_accumulatedDeltaTime = 0.0f;
		m_deltaStatsTimeElapsed += 1.0f;
	}

} // namespace NeneEngine
