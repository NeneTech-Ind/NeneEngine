#pragma once

#include "App/AppConfig.h"

#include <filesystem>

namespace NeneEngine
{
	struct LoadedAppConfig
	{
		AppConfig config{};
		std::filesystem::path path;
		std::filesystem::file_time_type lastWriteTime{};
	};

	[[nodiscard]] std::filesystem::path ResolveStartupAppConfigPath();
	[[nodiscard]] LoadedAppConfig LoadStartupAppConfig(const std::filesystem::path& configPath);
	[[nodiscard]] LoadedAppConfig LoadStartupAppConfig();
} // namespace NeneEngine
