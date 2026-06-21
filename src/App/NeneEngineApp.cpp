// NeneEngineApp.cpp

#include "App/NeneEngineApp.h"
#include "App/AppConfig.h"
#include "Core/CustomLogger.h"
#include "Core/PathResolver.h"
#include "Input/KeyCodeStrings.h"

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
		(void)title;
		try
		{
			return m_bootstrapService.Initialize(*this, m_gameStateMachine, m_world, m_runtimeConfigService,
			                                     m_windowRuntimeService, ResolveLogFilePath().string(), width, height);
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
		    [this]() { return GetFocusedInput(); });
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
