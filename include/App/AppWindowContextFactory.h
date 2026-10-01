// AppWindowContextFactory.h

#pragma once

#include "App/AppWindowContext.h"

#include <EASTL/optional.h>

namespace NeneEngine
{

	class AppWindowContextFactory final
	{
	  public:
		[[nodiscard]] eastl::optional<AppWindowContext> Create(uint32_t width, uint32_t height,
		                                                       const std::string& title, ECS::Entity cameraEntity,
		                                                       bool isMain) const;
	};

} // namespace NeneEngine
