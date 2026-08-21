// DebugConsole.h

#pragma once

#include <cstddef>
#include <cstdint>

namespace NeneEngine::DebugConsole
{

	void Initialize();
	void UpdateSpatialCullingStats(uint32_t total, uint32_t indexed, std::size_t candidates, uint32_t frustum,
	                               uint32_t submitted);

} // namespace NeneEngine::DebugConsole
