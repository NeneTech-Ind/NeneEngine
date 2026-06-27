// MeshRenderRuntimeComponent.h

#pragma once

#include "Graphics/Runtime/MeshRenderBinding.h"

#include <unordered_map>

namespace NeneEngine::ECS
{

	struct MeshRenderRuntimeComponent
	{
		std::unordered_map<uintptr_t, MeshRenderRuntimeBinding> bindingsByRenderer;
	};

} // namespace NeneEngine::ECS
