// AppSecondaryCameraService.h

#pragma once

#include "ECS/Entity.h"

#include <EASTL/vector.h>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class AppSecondaryCameraService final
	{
	  public:
		[[nodiscard]] eastl::vector<ECS::Entity> CreateAdditionalWindowCameras(ECS::World& world,
		                                                                       ECS::Entity primaryCameraEntity,
		                                                                       size_t count, uint32_t width,
		                                                                       uint32_t height) const;
	};

} // namespace NeneEngine
