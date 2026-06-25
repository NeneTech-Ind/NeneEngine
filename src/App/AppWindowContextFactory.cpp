// AppWindowContextFactory.cpp

#include "App/AppWindowContextFactory.h"

#include "Core/CustomLogger.h"
#include "Platform/Win32/Win32Window.h"
#include "Graphics/Backend/DiligentDX12Adapter.h"

namespace NeneEngine
{
	std::optional<AppWindowContext> AppWindowContextFactory::Create(uint32_t width, uint32_t height,
	                                                                const std::string& title,
	                                                                ECS::Entity cameraEntity, bool isMain) const
	{
		AppWindowContext windowContext{};
		windowContext.title = title;
		windowContext.cameraEntity = cameraEntity;
		windowContext.isMain = isMain;
		windowContext.window = eastl::make_unique<Win32Window>();
		if (!windowContext.window->Create(width, height, title))
		{
			NENE_LOG_ERROR("Failed to create window '{}'", title);
			return std::nullopt;
		}

		windowContext.renderer = eastl::make_unique<DiligentDX12Adapter>();
		if (!windowContext.renderer->Init(windowContext.window->GetHWND(), width, height))
		{
			NENE_LOG_ERROR("Failed to initialize renderer for window '{}'", title);
			return std::nullopt;
		}

		windowContext.inputManager.SetInputDevice(&windowContext.window->GetInput());
		windowContext.renderSystem = std::make_unique<ECS::RenderSystem>(windowContext.renderer.get(), cameraEntity);
		return windowContext;
	}

} // namespace NeneEngine
