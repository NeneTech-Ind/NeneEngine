// PhysicsSettings.h

#pragma once

#include <cstdint>

namespace NeneEngine::ECS
{

	struct PhysicsSettings
	{
		bool useJobSystem = true;
	};

	struct PhysicsStats
	{
		float stepMs = 0.0f;
		uint32_t bodyCount = 0;
		bool usedJobSystem = false;
	};

} // namespace NeneEngine::ECS
