// AppWindowContextFactory.cpp

#include "App/AppWindowContextFactory.h"

#include "Core/NeneLogger.h"
#include "Graphics/Backend/DiligentDX12Adapter.h"
#include "Platform/Win32/Win32Window.h"

namespace NeneEngine
{
	eastl::optional<AppWindowContext> AppWindowContextFactory::Create(uint32_t width, uint32_t height,
	                                                                  const std::string& title,
	                                                                  ECS::Entity cameraEntity, bool isMain)
	{
		AppWindowContext windowContext{};
		windowContext.title = title;
		windowContext.cameraEntity = cameraEntity;
		windowContext.isMain = isMain;
		windowContext.window = eastl::make_unique<Win32Window>();
		if (!windowContext.window->Create(width, height, title))
		{
			NENE_LOG_ERROR("Failed to create window '{}'", title);
			return eastl::nullopt;
		}

		if (!m_sharedDevice) m_sharedDevice = eastl::make_shared<DiligentDX12SharedDevice>();

		windowContext.renderer = eastl::make_unique<DiligentDX12Adapter>(m_sharedDevice);
		if (!windowContext.renderer->Init(windowContext.window->GetHWND(), width, height))
		{
			NENE_LOG_ERROR("Failed to initialize renderer for window '{}'", title);
			return eastl::nullopt;
		}

		windowContext.inputManager.SetInputDevice(&windowContext.window->GetInput());
		windowContext.renderSystem = eastl::make_unique<ECS::RenderSystem>(windowContext.renderer.get(), cameraEntity);
		return windowContext;
	}

} // namespace NeneEngine
