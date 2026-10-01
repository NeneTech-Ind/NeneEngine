// MeshRenderRuntimeComponent.h

#pragma once

#include "Graphics/Runtime/MeshRenderBinding.h"

#include <EASTL/unordered_map.h>

namespace NeneEngine::ECS
{

	struct MeshRenderRuntimeComponent
	{
		eastl::unordered_map<uintptr_t, MeshRenderRuntimeBinding> bindingsByRenderer;
	};

} // namespace NeneEngine::ECS
