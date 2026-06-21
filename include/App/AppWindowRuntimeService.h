// AppWindowRuntimeService.h

#pragma once

#include "App/AppConfig.h"
#include "Core/Delegate.h"
#include "ECS/Entity.h"
#include "ECS/Systems/ISystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "Input/InputDevice.h"
#include "Input/InputManager.h"
#include "Platform/IWindow.h"
#include "Graphics/Backend/IRenderAdapter.h"

#include <EASTL/unique_ptr.h>
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
		struct WindowContext
		{
			eastl::unique_ptr<IWindow> window;
			InputManager inputManager;
			eastl::unique_ptr<IRenderAdapter> renderer;
			std::unique_ptr<ECS::RenderSystem> renderSystem;
			DelegateHandle resizeHandle;
			ECS::Entity cameraEntity = ECS::NullEntity;
			std::string title;
		};

		bool CreateWindowContext(uint32_t width, uint32_t height, const std::string& title, ECS::Entity cameraEntity);
		std::vector<ECS::Entity> CreateAdditionalWindowCameras(ECS::Entity primaryCameraEntity, size_t count,
		                                                       uint32_t width, uint32_t height) const;
		void AddAppSystem(std::unique_ptr<ECS::ISystem> system);
		void HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height);

		ECS::World* m_world = nullptr;
		std::vector<WindowContext> m_windows;
		std::vector<std::unique_ptr<ECS::ISystem>> m_appSystems;
	};

} // namespace NeneEngine
