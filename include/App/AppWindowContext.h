// AppWindowContext.h

#pragma once

#include "Core/Delegate.h"
#include "ECS/Entity.h"
#include "ECS/Systems/RenderSystem.h"
#include "Input/InputManager.h"
#include "Platform/IWindow.h"
#include "Graphics/Backend/IRenderAdapter.h"

#include <EASTL/unique_ptr.h>
#include <memory>
#include <string>

namespace NeneEngine
{

	struct AppWindowContext
	{
		eastl::unique_ptr<IWindow> window;
		InputManager inputManager;
		eastl::unique_ptr<IRenderAdapter> renderer;
		std::unique_ptr<ECS::RenderSystem> renderSystem;
		DelegateHandle resizeHandle;
		ECS::Entity cameraEntity = ECS::NullEntity;
		bool isMain = false;
		std::string title;
	};

} // namespace NeneEngine
