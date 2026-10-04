// MeshRenderRuntimeComponent.h

#pragma once

#include "ECS/Entity.h"
#include "Graphics/Runtime/MeshRenderBinding.h"

#include <EASTL/unordered_map.h>

namespace NeneEngine
{
	class IRenderAdapter;
}

namespace NeneEngine::ECS
{

	class World;

	struct MeshRenderRuntimeComponent
	{
		eastl::unordered_map<uintptr_t, MeshRenderRuntimeBinding> bindingsByRenderer;
	};

	void BindMeshRenderRuntime(World& world, Entity entity, const MeshRenderRuntimeBinding& binding,
	                           const IRenderAdapter* renderer = nullptr);
	[[nodiscard]] const MeshRenderRuntimeBinding* GetMeshRenderRuntimeBinding(const World& world, Entity entity,
	                                                                          const IRenderAdapter* renderer = nullptr);
	void ClearMeshRenderRuntimeBinding(World& world, Entity entity);

} // namespace NeneEngine::ECS
