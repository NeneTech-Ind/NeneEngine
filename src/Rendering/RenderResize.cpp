// RenderResize.cpp

#include "Rendering/RenderResize.h"

#include "Core/CustomLogger.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/World.h"
#include "RenderAdapters/IRenderAdapter.h"

namespace NeneEngine
{

	void ResizeRenderResources(IRenderAdapter& renderer, ECS::World& world, uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			NENE_LOG_WARN("ResizeRenderResources: ignored invalid size {}x{}", width, height);
			return;
		}

		renderer.Resize(width, height);

		const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		auto cameraView = world.GetRegistry().view<ECS::CameraComponent>();

		for (auto entity : cameraView)
		{
			auto& camera = cameraView.get<ECS::CameraComponent>(entity);
			if (camera.isPrimary) camera.aspectRatio = aspectRatio;
		}
	}

} // namespace NeneEngine
