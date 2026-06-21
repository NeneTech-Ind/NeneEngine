// AppBootstrapService.h

#pragma once

#include "App/AppConfig.h"

#include <cstdint>
#include <functional>
#include <string>

namespace NeneEngine
{
	class AppRuntimeConfigService;
	class AppWindowRuntimeService;
	class GameStateMachine;
	class NeneEngineApp;

	namespace ECS
	{
		class World;
	}

	class AppBootstrapService final
	{
	  public:
		using ApplyConfigCallback = std::function<void(const AppConfig&)>;

		bool Initialize(NeneEngineApp& app, GameStateMachine& gameStateMachine, ECS::World& world,
		                AppRuntimeConfigService& runtimeConfigService, AppWindowRuntimeService& windowRuntimeService,
		                ApplyConfigCallback applyRuntimeConfig, const std::string& logFilePath, uint32_t width,
		                uint32_t height);
	};

} // namespace NeneEngine
