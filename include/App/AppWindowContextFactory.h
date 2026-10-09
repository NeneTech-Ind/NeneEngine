// AppWindowContextFactory.h

#pragma once

#include "App/AppWindowContext.h"

#include <EASTL/optional.h>
#include <EASTL/shared_ptr.h>

namespace NeneEngine
{

	struct DiligentDX12SharedDevice;

	class AppWindowContextFactory final
	{
	  public:
		[[nodiscard]] eastl::optional<AppWindowContext> Create(uint32_t width, uint32_t height,
		                                                       const std::string& title, ECS::Entity cameraEntity,
		                                                       bool isMain);

	  private:
		eastl::shared_ptr<DiligentDX12SharedDevice> m_sharedDevice;
	};

} // namespace NeneEngine
