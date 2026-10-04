// AppRuntimeConfigService.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppStartupConfig.h"

#include <EASTL/functional.h>

namespace NeneEngine
{

	class AppRuntimeConfigService final
	{
	  public:
		using ApplyConfigCallback = eastl::function<void(const AppConfig&)>;

		void LoadStartupConfig();
		[[nodiscard]] const AppConfig& GetConfig() const { return m_loadedAppConfig.config; }
		void Update(float deltaTime, ApplyConfigCallback applyRuntimeConfig);

	  private:
		LoadedAppConfig m_loadedAppConfig{};
		float m_configReloadAccumulator = 0.0f;
	};

} // namespace NeneEngine
