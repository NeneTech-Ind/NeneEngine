// AppFrameLoopService.cpp

#include "App/AppFrameLoopService.h"

#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "Core/GameTimer.h"
#include "Core/NeneLogger.h"
#include "Core/Profiler.h"
#include "Core/ResourceLoadQueue.h"
#include "Editor/EditorLayer.h"
#include "GameStates/GameStateMachine.h"
#include "Input/InputDevice.h"
#include "Input/InputManager.h"

#include <algorithm>
#include <exception>
#include <format>

namespace NeneEngine
{
	namespace
	{
		constexpr uint32_t kPausedWaitTimeoutMs = 100;
		constexpr float kMaxFrameDeltaTime = 0.1f;
	} // namespace

	void AppFrameLoopService::Run(std::atomic<bool>& running, GameTimer& timer, GameStateMachine& gameStateMachine,
	                              InputManager& inputManager, AppRuntimeConfigService& runtimeConfigService,
	                              AppWindowRuntimeService& windowRuntimeService, ResourceLoadQueue& resourceLoads,
	                              EditorLayer& editorLayer, ApplyConfigCallback applyRuntimeConfig,
	                              FocusedInputCallback getFocusedInput)
	{
		running = true;
		timer.Reset();

		bool drawingResizeFrame = false;
		std::exception_ptr resizeError;
		const IWindow::SizeMoveHandler handleSizeMove = [&](SizeMoveEvent event)
		{
			if (event == SizeMoveEvent::Begin)
			{
				timer.Stop();
				return;
			}
			if (drawingResizeFrame || resizeError || !running.load() || windowRuntimeService.IsMainWindowClosed()) return;

			drawingResizeFrame = true;
			try
			{
				NENE_PROFILE_SCOPE("Live Resize Redraw");
				editorLayer.DrawFrame(false);
				windowRuntimeService.Render(false);
			}
			catch (...)
			{
				resizeError = std::current_exception();
			}
			drawingResizeFrame = false;
		};

		while (running.load() && !windowRuntimeService.IsMainWindowClosed())
		{
			NENE_PROFILE_SCOPE("Frame");
			windowRuntimeService.PumpWindowMessages(handleSizeMove);
			if (resizeError) std::rethrow_exception(resizeError);
			if (!running.load() || windowRuntimeService.IsMainWindowClosed()) break;
			resourceLoads.Pump();

			if (windowRuntimeService.AreAllWindowsMinimized())
			{
				timer.Stop();
				windowRuntimeService.WaitForWindowMessages(kPausedWaitTimeoutMs);
				continue;
			}

			timer.Start();
			timer.Tick();
			const float deltaTime = (std::min)(timer.GetDeltaTime(), kMaxFrameDeltaTime);

			const bool isGameplayRunning = editorLayer.IsGameplayRunning();
			InputPhase(deltaTime, inputManager, windowRuntimeService, getFocusedInput);
			GameplayPhase(deltaTime, timer, gameStateMachine, runtimeConfigService, applyRuntimeConfig,
			              isGameplayRunning);
			EditorPhase(editorLayer);
			SyncPhase(deltaTime);
			{
				NENE_PROFILE_SCOPE("Render");
				windowRuntimeService.Render();
			}
			EndFramePhase(timer, windowRuntimeService);
			NENE_PROFILE_FRAME();
		}
	}

	void AppFrameLoopService::InputPhase(float deltaTime, InputManager& inputManager,
	                                     AppWindowRuntimeService& windowRuntimeService,
	                                     FocusedInputCallback getFocusedInput)
	{
		NENE_PROFILE_SCOPE("Input");
		inputManager.SetInputDevice(getFocusedInput());
		inputManager.UpdateState();
		windowRuntimeService.UpdateInputManagers();
		windowRuntimeService.UpdateWindowSystems(deltaTime);
	}

	void AppFrameLoopService::GameplayPhase(float deltaTime, GameTimer& timer, GameStateMachine& gameStateMachine,
	                                        AppRuntimeConfigService& runtimeConfigService,
	                                        ApplyConfigCallback applyRuntimeConfig, bool isGameplayRunning)
	{
		NENE_PROFILE_SCOPE("Gameplay");
		if (isGameplayRunning)
		{
			gameStateMachine.HandleInput();
			gameStateMachine.Update(deltaTime);
		}
		LogDeltaTimeStats(timer, deltaTime);
		runtimeConfigService.Update(deltaTime, applyRuntimeConfig);
	}

	void AppFrameLoopService::EditorPhase(EditorLayer& editorLayer)
	{
		NENE_PROFILE_SCOPE("Editor");
		editorLayer.Update();
	}

	void AppFrameLoopService::SyncPhase(float /*deltaTime*/)
	{
		// Reserved explicit phase for synchronizing physics/runtime state back into scene transforms.
		NENE_PROFILE_SCOPE("Sync");
	}

	void AppFrameLoopService::EndFramePhase(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService)
	{
		NENE_PROFILE_SCOPE("EndFrame");
		CalculateFrameStats(timer, windowRuntimeService);
		windowRuntimeService.EndFrameInputs();
	}

	void AppFrameLoopService::CalculateFrameStats(GameTimer& timer, AppWindowRuntimeService& windowRuntimeService)
	{
		++m_frameCount;
		if ((timer.GetTotalTime() - m_frameStatsTimeElapsed) < 1.0f) return;

		const float fps = static_cast<float>(m_frameCount);
		const float mspf = 1000.0f / fps;

		windowRuntimeService.UpdateFrameStatsText(std::format(" | fps: {:.0f} | mspf: {:.6f}", fps, mspf));

		m_frameCount = 0;
		m_frameStatsTimeElapsed += 1.0f;
	}

	void AppFrameLoopService::LogDeltaTimeStats(GameTimer& timer, float deltaTime)
	{
		++m_deltaSampleCount;
		m_accumulatedDeltaTime += deltaTime;
		m_lastDeltaTime = deltaTime;

		if ((timer.GetTotalTime() - m_deltaStatsTimeElapsed) < 1.0f) return;

		const float averageDeltaTime =
		    m_deltaSampleCount > 0 ? m_accumulatedDeltaTime / static_cast<float>(m_deltaSampleCount) : 0.0f;

		NENE_LOG_DEBUG("deltaTime: last={:.6f} s ({:.3f} ms), avg={:.6f} s ({:.3f} ms), samples={}",
		               m_lastDeltaTime, m_lastDeltaTime * 1000.0f, averageDeltaTime, averageDeltaTime * 1000.0f,
		               m_deltaSampleCount);

		m_deltaSampleCount = 0;
		m_accumulatedDeltaTime = 0.0f;
		m_deltaStatsTimeElapsed += 1.0f;
	}

} // namespace NeneEngine
