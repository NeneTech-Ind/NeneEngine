#pragma once

#include "ECS/World.h"

#include <span>

namespace NeneEngine
{
	class IRenderAdapter;

	void RunDemoBootstrap(ECS::World& world, std::span<IRenderAdapter* const> renderers);
} // namespace NeneEngine
