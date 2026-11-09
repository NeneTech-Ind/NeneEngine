#pragma once

#include "ECS/World.h"

#include <EASTL/span.h>

namespace NeneEngine
{
	class IRenderAdapter;
	class ResourceLoadQueue;

	void RunDemoBootstrap(ECS::World& world, eastl::span<IRenderAdapter* const> renderers, ResourceLoadQueue& loads);
} // namespace NeneEngine
