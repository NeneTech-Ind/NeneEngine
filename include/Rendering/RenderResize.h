// RenderResize.h

#pragma once

#include <cstdint>

namespace NeneEngine
{

	class IRenderAdapter;

	namespace ECS
	{
		class World;
	}

	void ResizeRenderResources(IRenderAdapter& renderer, ECS::World& world, uint32_t width, uint32_t height);

} // namespace NeneEngine
