// AppRuntimeConfigService.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppStartupConfigService.h"

#include <functional>

namespace NeneEngine
{

	class AppRuntimeConfigService final
	{
	  public:
		using ApplyConfigCallback = std::function<void(const AppConfig&)>;

		void LoadStartupConfig();
		[[nodiscard]] const AppConfig& GetConfig() const { return m_loadedAppConfigState.config; }
		void Update(float deltaTime, ApplyConfigCallback applyRuntimeConfig);

	  private:
		LoadedAppConfigState m_loadedAppConfigState{};
		float m_configReloadAccumulator = 0.0f;
	};

} // namespace NeneEngine
