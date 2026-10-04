// MeshRenderBinding.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine
{

	struct MeshRenderRuntimeBinding
	{
		MeshId meshId{};
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
	};

} // namespace NeneEngine
