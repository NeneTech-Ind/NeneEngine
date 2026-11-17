// Application.h

#pragma once

#include "App/AppBootstrapService.h"
#include "App/AppConfig.h"
#include "App/AppFrameLoopService.h"
#include "App/AppRuntimeConfigService.h"
#include "App/AppWindowRuntimeService.h"
#include "Core/GameTimer.h"
#include "Core/JobSystem.h"
#include "Core/ResourceLoadQueue.h"
#include "ECS/World.h"
#include "Editor/EditorLayer.h"
#include "GameStates/GameStateMachine.h"
#include "Input/InputManager.h"
#include "Scene/Prefab/PrefabLibrary.h"
#include "Scripting/ScriptEngine.h"

#include <atomic>

namespace NeneEngine
{

	class NeneEngineApp
	{
	  public:
		NeneEngineApp();
		~NeneEngineApp();

		bool Init(uint32_t width = 1280, uint32_t height = 720);
		void Run();
		void RequestShutdown();
		void RequestExit();
		InputManager& GetInputManager() { return m_inputManager; }
		const InputManager& GetInputManager() const { return m_inputManager; }
		ResourceLoadQueue& GetResourceLoads() { return m_resourceLoads; }
		SceneAssetResolver& GetSceneAssets() { return m_sceneAssets; }
		ScriptEngine& GetScriptEngine() { return m_scriptEngine; }
		PrefabLibrary& GetPrefabLibrary() { return m_prefabLibrary; }

	  private:
		JobSystem m_jobSystem;
		ResourceLoadQueue m_resourceLoads{m_jobSystem};
		SceneAssetResolver m_sceneAssets{&m_resourceLoads};
		InputManager m_inputManager;
		ScriptEngine m_scriptEngine;
		GameTimer m_timer;
		GameStateMachine m_gameStateMachine;
		ECS::World m_world;
		PrefabLibrary m_prefabLibrary{m_world};
		AppBootstrapService m_bootstrapService;
		AppFrameLoopService m_frameLoopService;
		AppRuntimeConfigService m_runtimeConfigService;
		AppWindowRuntimeService m_windowRuntimeService;
		EditorLayer m_editorLayer;

		std::atomic<bool> m_running{false};

		void ApplyRuntimeAppConfig(const AppConfig& config);
		void OnEditorModeChanged(EditorMode mode);
	};

} // namespace NeneEngine
