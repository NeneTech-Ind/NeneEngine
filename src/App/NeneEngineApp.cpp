// NeneEngineApp.cpp

#include "App/NeneEngineApp.h"
#include "App/AppConfig.h"
#include "App/AppInputBindingUtils.h"
#include "Core/CustomLogger.h"
#include "Core/PathResolver.h"

#include <filesystem>
#include <stdexcept>

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
		m_gameStateMachine.Clear();
		m_windowRuntimeService.Shutdown();
		CustomLogger::GetInstance().Shutdown();
	}

	bool NeneEngineApp::Init(uint32_t width, uint32_t height, const std::string& title)
	{
		(void)title;
		try
		{
			return m_bootstrapService.Initialize(*this, m_gameStateMachine, m_world, m_runtimeConfigService,
			                                     m_windowRuntimeService,
			                                     [this](const AppConfig& config) { ApplyRuntimeAppConfig(config); },
			                                     ResolveLogFilePath().string(), width, height);
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
		m_frameLoopService.Run(
		    m_running, m_isPaused, m_timer, m_gameStateMachine, m_inputManager, m_runtimeConfigService,
		    m_windowRuntimeService, [this](const AppConfig& config) { ApplyRuntimeAppConfig(config); },
		    [this]() { return m_windowRuntimeService.GetFocusedInput(); });
	}

	void NeneEngineApp::RequestShutdown()
	{
		m_running = false;
	}

} // namespace NeneEngine
