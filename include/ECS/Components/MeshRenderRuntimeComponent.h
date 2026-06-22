// MeshRenderRuntimeComponent.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine::ECS
{

	struct MeshRenderRuntimeComponent
	{
		MeshId meshId{};
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
	};

} // namespace NeneEngine::ECS
