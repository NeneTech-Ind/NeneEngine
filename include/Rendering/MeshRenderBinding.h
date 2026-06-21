// MeshRenderBinding.h

#pragma once

#include "ECS/Entity.h"
#include "Rendering/RenderTypes.h"

namespace NeneEngine
{
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

	void BindMeshRenderRuntime(ECS::World& world, ECS::Entity entity, const MeshRenderRuntimeBinding& binding);
	void ClearMeshRenderRuntimeBinding(ECS::World& world, ECS::Entity entity);

} // namespace NeneEngine
