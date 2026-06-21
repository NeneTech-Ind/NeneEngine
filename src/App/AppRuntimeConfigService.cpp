// AppRuntimeConfigService.cpp

#include "App/AppRuntimeConfigService.h"

#include "App/AppRuntimeConfigPolicy.h"
#include "Core/CustomLogger.h"

#include <filesystem>

namespace NeneEngine
{
	namespace
	{
		constexpr float kConfigReloadIntervalSeconds = 0.5f;
	}

	void AppRuntimeConfigService::LoadStartupConfig()
	{
		m_loadedAppConfigState = LoadStartupAppConfigState();
		m_configReloadAccumulator = 0.0f;
	}

	void AppRuntimeConfigService::Update(float deltaTime, ApplyConfigCallback applyRuntimeConfig)
	{
		m_configReloadAccumulator += deltaTime;
		if (m_configReloadAccumulator < kConfigReloadIntervalSeconds) return;

		m_configReloadAccumulator = 0.0f;

		const std::filesystem::path resolvedConfigPath = ResolveStartupAppConfigPath();
		const bool pathChanged = resolvedConfigPath != m_loadedAppConfigState.path;

		if (pathChanged)
		{
			NENE_LOG_INFO("App config path updated to '{}'", resolvedConfigPath.string());
		}

		if (!std::filesystem::exists(resolvedConfigPath)) return;

		const auto currentWriteTime = std::filesystem::last_write_time(resolvedConfigPath);
		if (!pathChanged && currentWriteTime == m_loadedAppConfigState.lastWriteTime) return;

		const LoadedAppConfigState resolvedConfigState = LoadStartupAppConfigState(resolvedConfigPath);

		const auto hotReloadResult =
		    EvaluateAppConfigHotReload(m_loadedAppConfigState.config, resolvedConfigState.config);
		if (hotReloadResult.requiresRestart)
		{
			NENE_LOG_WARN("App config hot-reload: window definitions changed, but window creation, resizing, titles, "
			              "and main-window reassignment require application restart");
		}

		applyRuntimeConfig(hotReloadResult.runtimeAppliedConfig);
		m_loadedAppConfigState = resolvedConfigState;

		NENE_LOG_INFO("App config hot-reloaded from '{}'; applied runtime-supported changes only",
		              m_loadedAppConfigState.path.string());
	}

} // namespace NeneEngine
