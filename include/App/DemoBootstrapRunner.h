#pragma once

#include "ECS/World.h"

#include <EASTL/span.h>

namespace NeneEngine
{
	class IRenderAdapter;

	void RunDemoBootstrap(ECS::World& world, eastl::span<IRenderAdapter* const> renderers);
} // namespace NeneEngine
