#include "App/AppStartupConfig.h"

#include <filesystem>
#include <system_error>

namespace NeneEngine
{
	std::filesystem::path ResolveStartupAppConfigPath()
	{
		return GetDefaultAppConfigPath();
	}

	LoadedAppConfig LoadStartupAppConfig(const std::filesystem::path& configPath)
	{
		LoadedAppConfig state{};
		state.path = configPath;
		state.config = LoadAppConfig(state.path);

		std::error_code fileError;
		if (std::filesystem::exists(state.path, fileError))
		{
			state.lastWriteTime = std::filesystem::last_write_time(state.path, fileError);
		}

		return state;
	}

	LoadedAppConfig LoadStartupAppConfig()
	{
		return LoadStartupAppConfig(ResolveStartupAppConfigPath());
	}
} // namespace NeneEngine
