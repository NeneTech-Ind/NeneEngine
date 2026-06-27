// MeshRenderBinding.h

#pragma once

#include "ECS/Entity.h"
#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine
{
	class IRenderAdapter;

	namespace ECS
	{
		class World;
	}

	struct MeshRenderRuntimeBinding
	{
		MeshId meshId{};
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
	};

	void BindMeshRenderRuntime(ECS::World& world, ECS::Entity entity, const MeshRenderRuntimeBinding& binding,
	                           const IRenderAdapter* renderer = nullptr);
	[[nodiscard]] const MeshRenderRuntimeBinding* GetMeshRenderRuntimeBinding(const ECS::World& world, ECS::Entity entity,
	                                                                          const IRenderAdapter* renderer = nullptr);
	void ClearMeshRenderRuntimeBinding(ECS::World& world, ECS::Entity entity);

} // namespace NeneEngine
