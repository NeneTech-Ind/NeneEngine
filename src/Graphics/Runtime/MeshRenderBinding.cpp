#include "Graphics/Runtime/MeshRenderBinding.h"

#include "ECS/Components/MeshRenderRuntimeComponent.h"
#include "ECS/World.h"

namespace NeneEngine
{

	void BindMeshRenderRuntime(ECS::World& world, ECS::Entity entity, const MeshRenderRuntimeBinding& binding)
	{
		auto* runtime = world.GetComponent<ECS::MeshRenderRuntimeComponent>(entity);
		if (runtime == nullptr) runtime = &world.AddComponent<ECS::MeshRenderRuntimeComponent>(entity);

		runtime->meshId = binding.meshId;
		runtime->materialId = binding.materialId;
		runtime->shaderId = binding.shaderId;
		runtime->textureId = binding.textureId;
	}

	void ClearMeshRenderRuntimeBinding(ECS::World& world, ECS::Entity entity)
	{
		if (world.HasComponent<ECS::MeshRenderRuntimeComponent>(entity))
			world.RemoveComponent<ECS::MeshRenderRuntimeComponent>(entity);
	}

} // namespace NeneEngine
