// GeometryBounds.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine
{
	[[nodiscard]] std::optional<LocalAabb> GetPrimitiveLocalBounds(PrimitiveType primitive);
	[[nodiscard]] std::optional<LocalAabb> ComputeLocalBounds(const MeshData& mesh);
} // namespace NeneEngine
