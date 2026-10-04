// AppRuntimeConfigService.cpp

#include "App/AppRuntimeConfigService.h"

#include "App/AppRuntimeConfigPolicy.h"
#include "Core/NeneLogger.h"

#include <filesystem>
#include <system_error>

namespace NeneEngine
{
	namespace
	{
		constexpr float kConfigReloadIntervalSeconds = 0.5f;
	}

	void AppRuntimeConfigService::LoadStartupConfig()
	{
		m_loadedAppConfig = LoadStartupAppConfig();
		m_configReloadAccumulator = 0.0f;
	}

	void AppRuntimeConfigService::Update(float deltaTime, ApplyConfigCallback applyRuntimeConfig)
	{
		m_configReloadAccumulator += deltaTime;
		if (m_configReloadAccumulator < kConfigReloadIntervalSeconds) return;

		m_configReloadAccumulator = 0.0f;

		const std::filesystem::path resolvedConfigPath = ResolveStartupAppConfigPath();
		const bool pathChanged = resolvedConfigPath != m_loadedAppConfig.path;

		if (pathChanged)
		{
			NENE_LOG_INFO("App config path updated to '{}'", resolvedConfigPath.string());
		}

		std::error_code fileError;
		if (!std::filesystem::exists(resolvedConfigPath, fileError))
		{
			if (fileError)
				NENE_LOG_WARN("App config hot-reload: failed to inspect '{}': {}", resolvedConfigPath.string(),
				              fileError.message());
			return;
		}

		const auto currentWriteTime = std::filesystem::last_write_time(resolvedConfigPath, fileError);
		if (fileError)
		{
			NENE_LOG_WARN("App config hot-reload: failed to read timestamp for '{}': {}", resolvedConfigPath.string(),
			              fileError.message());
			return;
		}

		if (!pathChanged && currentWriteTime == m_loadedAppConfig.lastWriteTime) return;

		const LoadedAppConfig resolvedConfig = LoadStartupAppConfig(resolvedConfigPath);

		const auto hotReloadResult = EvaluateAppConfigHotReload(m_loadedAppConfig.config, resolvedConfig.config);
		if (hotReloadResult.requiresRestart)
		{
			NENE_LOG_WARN("App config hot-reload: window definitions changed, but window creation, resizing, titles, "
			              "and main-window reassignment require application restart");
		}

		applyRuntimeConfig(hotReloadResult.runtimeAppliedConfig);
		m_loadedAppConfig = resolvedConfig;

		NENE_LOG_INFO("App config hot-reloaded from '{}'; applied runtime-supported changes only",
		              m_loadedAppConfig.path.string());
	}

} // namespace NeneEngine
