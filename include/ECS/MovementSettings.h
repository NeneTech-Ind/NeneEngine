// MovementSettings.h

#pragma once

#include <cstdint>

namespace NeneEngine::ECS
{

	struct MovementSettings
	{
		bool useJobSystem = true;
	};

	struct MovementStats
	{
		float updateMs = 0.0f;
		uint32_t entityCount = 0;
		bool usedJobSystem = false;
	};

} // namespace NeneEngine::ECS
