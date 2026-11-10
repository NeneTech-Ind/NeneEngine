// NeneEngineApp.cpp

#include "App/NeneEngineApp.h"
#include "App/AppConfig.h"
#include "App/AppInputBindings.h"
#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Core/Profiler.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace NeneEngine
{
	namespace
	{
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
		m_resourceLoads.Shutdown();
		m_jobSystem.Shutdown();
		m_editorLayer.Shutdown();
		m_gameStateMachine.Clear();
		NeneLogger::GetInstance().Shutdown();
	}

	bool NeneEngineApp::Init(uint32_t width, uint32_t height)
	{
		NENE_PROFILE_THREAD_NAME("Main");
		NENE_PROFILE_SCOPE("App Init");

		try
		{
			m_windowRuntimeService.SetAssetResolver(m_sceneAssets);
			m_windowRuntimeService.SetJobSystem(m_jobSystem);
			if (!m_bootstrapService.Initialize(
			        *this, m_jobSystem, m_gameStateMachine, m_world, m_runtimeConfigService, m_windowRuntimeService,
			        [this](const AppConfig& config) { ApplyRuntimeAppConfig(config); }, ResolveLogFilePath().string(),
			        width, height))
				return false;

			if (auto* renderer = m_windowRuntimeService.GetMainRenderer())
			{
				// Compile the built-in material before frames start, just like the primitive pipelines.
				NENE_PROFILE_SCOPE("Default Shader Warmup");
				const std::string shaderPath = "assets/shaders/textured_mesh.shader";
				m_sceneAssets.Shader(shaderPath, *renderer, JobPriority::High);
				while (m_sceneAssets.GetShaderState(shaderPath, *renderer) == ResourceLoadState::Loading)
				{
					m_resourceLoads.Pump(1);
					std::this_thread::sleep_for(std::chrono::milliseconds{1});
				}
			}

			m_editorLayer.Initialize(
			    m_world, m_windowRuntimeService.GetMainImGuiBackend(), m_windowRuntimeService.GetMainWindowInput(),
			    m_windowRuntimeService.GetMainCameraEntity(), [this]() { RequestShutdown(); },
			    [this](EditorMode mode) { OnEditorModeChanged(mode); });
			m_editorLayer.SetRenderer(m_windowRuntimeService.GetMainRenderer());
			m_editorLayer.SetResourceLoading(m_resourceLoads, m_sceneAssets);
			m_windowRuntimeService.SetMainWindowCloseRequestHandler(
			    [this]() { return m_editorLayer.ConfirmDiscardChanges(); });
			return true;
		}
		catch (const std::exception& e)
		{
			NENE_LOG_ERROR("Init failed: {}", e.what());

			return false;
		}
	}

	void NeneEngineApp::ApplyRuntimeAppConfig(const AppConfig& config)
	{
		ApplyInputBindings(m_inputManager, config.input, "app input manager");
		m_windowRuntimeService.ApplyRuntimeConfig(config);
	}

	void NeneEngineApp::Run()
	{
		try
		{
			m_frameLoopService.Run(
			    m_running, m_timer, m_gameStateMachine, m_inputManager, m_runtimeConfigService, m_windowRuntimeService,
			    m_resourceLoads, m_editorLayer, [this](const AppConfig& config) { ApplyRuntimeAppConfig(config); },
			    [this]() { return m_windowRuntimeService.GetFocusedInput(); });
		}
		catch (const std::exception& e)
		{
			NENE_LOG_ERROR("Frame loop terminated by an exception: {}", e.what());
			throw;
		}
		catch (...)
		{
			NENE_LOG_ERROR("Frame loop terminated by an unknown exception");
			throw;
		}
	}

	void NeneEngineApp::OnEditorModeChanged(EditorMode mode)
	{
		if (mode == EditorMode::Edit) m_gameStateMachine.PopToBaseState();
	}

	void NeneEngineApp::RequestShutdown()
	{
		m_running = false;
	}

	void NeneEngineApp::RequestExit()
	{
		m_editorLayer.RequestExit();
	}

} // namespace NeneEngine
