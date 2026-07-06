#include "App/AppStartupConfigService.h"

#include <filesystem>
#include <system_error>

namespace NeneEngine
{
	std::filesystem::path ResolveStartupAppConfigPath()
	{
		return DefaultAppConfigPath();
	}

	LoadedAppConfigState LoadStartupAppConfigState(const std::filesystem::path& configPath)
	{
		LoadedAppConfigState state{};
		state.path = configPath;
		state.config = LoadAppConfig(state.path);

		std::error_code fileError;
		if (std::filesystem::exists(state.path, fileError))
		{
			state.lastWriteTime = std::filesystem::last_write_time(state.path, fileError);
		}

		return state;
	}

	LoadedAppConfigState LoadStartupAppConfigState()
	{
		return LoadStartupAppConfigState(ResolveStartupAppConfigPath());
	}
} // namespace NeneEngine
