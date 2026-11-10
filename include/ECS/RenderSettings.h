// RenderSettings.h

#pragma once

#include <cstdint>

namespace NeneEngine::ECS
{

	struct RenderSettings
	{
		bool useJobSystem = true;
	};

	struct RenderStats
	{
		float preparationMs = 0.0f;
		uint32_t transformCount = 0;
		uint32_t hierarchyDepth = 0;
		uint32_t itemCount = 0;
		bool usedJobSystem = false;
	};

} // namespace NeneEngine::ECS
