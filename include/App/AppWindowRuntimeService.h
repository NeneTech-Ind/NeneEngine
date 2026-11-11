// AppWindowRuntimeService.h

#pragma once

#include "App/AppConfig.h"
#include "App/AppSecondaryCameraFactory.h"
#include "App/AppWindowContext.h"
#include "App/AppWindowContextFactory.h"
#include "ECS/Systems/ISystem.h"
#include "Input/InputDevice.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class AppWindowRuntimeService final
	{
	  public:
		void SetAssetResolver(SceneAssetResolver& assets) { m_windowContextFactory.SetAssetResolver(assets); }
		void SetJobSystem(JobSystem& jobs) { m_windowContextFactory.SetJobSystem(jobs); }
		~AppWindowRuntimeService();

		bool Initialize(const AppConfig& config, ECS::World& world, ECS::Entity primaryCameraEntity, uint32_t width,
		                uint32_t height);
		void Shutdown();

		[[nodiscard]] bool IsMainWindowClosed() const;
		[[nodiscard]] bool AreAllWindowsMinimized() const;
		void PumpWindowMessages();
		void WaitForWindowMessages(uint32_t timeoutMs);
		void UpdateInputManagers();
		void UpdateWindowSystems(float deltaTime);
		void Render();
		void EndFrameInputs();
		void UpdateFrameStatsText(const std::string& statsText);
		void ApplyRuntimeConfig(const AppConfig& config);
		[[nodiscard]] InputDevice* GetFocusedInput();
		[[nodiscard]] const InputDevice* GetFocusedInput() const;
		[[nodiscard]] IImGuiBackend* GetMainImGuiBackend();
		[[nodiscard]] IRenderAdapter* GetMainRenderer();
		[[nodiscard]] InputDevice* GetMainWindowInput();
		[[nodiscard]] ECS::Entity GetMainCameraEntity();
		void SetMainWindowCloseRequestHandler(IWindow::CloseRequestHandler handler);
		[[nodiscard]] eastl::vector<IRenderAdapter*> GetRenderers() const;

	  private:
		bool CreateWindowContext(uint32_t width, uint32_t height, const std::string& title, ECS::Entity cameraEntity,
		                         bool isMain);
		void AddAppSystem(eastl::unique_ptr<ECS::ISystem> system);
		void HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height);
		[[nodiscard]] AppWindowContext* FindMainWindow();

		ECS::World* m_world = nullptr;
		AppSecondaryCameraFactory m_secondaryCameraFactory;
		AppWindowContextFactory m_windowContextFactory;
		eastl::vector<AppWindowContext> m_windows;
		eastl::vector<eastl::unique_ptr<ECS::ISystem>> m_appSystems;
	};

} // namespace NeneEngine
