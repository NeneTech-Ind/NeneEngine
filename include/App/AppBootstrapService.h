// AppBootstrapService.h

#pragma once

#include <cstdint>
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
		bool Initialize(NeneEngineApp& app, GameStateMachine& gameStateMachine, ECS::World& world,
		                AppRuntimeConfigService& runtimeConfigService, AppWindowRuntimeService& windowRuntimeService,
		                const std::string& logFilePath, uint32_t width, uint32_t height);
	};

} // namespace NeneEngine
