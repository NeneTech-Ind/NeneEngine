// AppWindowRuntimeService.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppSecondaryCameraService.h"
#include "App/AppWindowContext.h"
#include "App/AppWindowContextFactory.h"
#include "ECS/Systems/ISystem.h"
#include "Input/InputDevice.h"

#include <memory>
#include <vector>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class AppWindowRuntimeService final
	{
	  public:
		~AppWindowRuntimeService();

		bool Initialize(const AppConfig& config, ECS::World& world, ECS::Entity primaryCameraEntity, uint32_t width,
		                uint32_t height);
		void Shutdown();

		[[nodiscard]] bool AreAllWindowsClosed() const;
		void PumpWindowMessages();
		void UpdateInputManagers();
		void UpdateWindowSystems(float deltaTime);
		void Render();
		void EndFrameInputs();
		void UpdateFrameStatsText(const std::wstring& fpsText, const std::wstring& mspfText) const;
		void ApplyRuntimeConfig(const AppConfig& config);
		[[nodiscard]] InputDevice* GetFocusedInput();
		[[nodiscard]] const InputDevice* GetFocusedInput() const;
		[[nodiscard]] IRenderAdapter* GetPrimaryRenderer();

	  private:
		bool CreateWindowContext(uint32_t width, uint32_t height, const std::string& title, ECS::Entity cameraEntity,
		                         bool isMain);
		void AddAppSystem(std::unique_ptr<ECS::ISystem> system);
		void HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height);

		ECS::World* m_world = nullptr;
		AppSecondaryCameraService m_secondaryCameraService;
		AppWindowContextFactory m_windowContextFactory;
		std::vector<AppWindowContext> m_windows;
		std::vector<std::unique_ptr<ECS::ISystem>> m_appSystems;
	};

} // namespace NeneEngine
