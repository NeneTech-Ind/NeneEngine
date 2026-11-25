// MeshRenderBinding.h

#pragma once

#include "Core/ResourceLoadQueue.h"
#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine
{
	struct MeshGeometryBinding
	{
		GPUMesh mesh{};
		ResourceLoadState state = ResourceLoadState::Ready;
	};

	struct MeshRenderRuntimeBinding
	{
		MeshId meshId{};
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
		ResourceLoadState meshState = ResourceLoadState::Ready;
		std::optional<LocalAabb> localBounds{};
	};

} // namespace NeneEngine
