#include "Graphics/Runtime/MeshRenderBinding.h"

#include "ECS/Components/MeshRenderRuntimeComponent.h"
#include "ECS/World.h"

namespace NeneEngine
{
	namespace
	{
		uintptr_t GetRendererBindingKey(const IRenderAdapter* renderer)
		{
			return renderer != nullptr ? reinterpret_cast<uintptr_t>(renderer) : 0;
		}
	} // namespace

	void BindMeshRenderRuntime(ECS::World& world, ECS::Entity entity, const MeshRenderRuntimeBinding& binding,
	                           const IRenderAdapter* renderer)
	{
		auto* runtime = world.GetComponent<ECS::MeshRenderRuntimeComponent>(entity);
		if (runtime == nullptr) runtime = &world.AddComponent<ECS::MeshRenderRuntimeComponent>(entity);

		runtime->bindingsByRenderer[GetRendererBindingKey(renderer)] = binding;
	}

	const MeshRenderRuntimeBinding* GetMeshRenderRuntimeBinding(const ECS::World& world, ECS::Entity entity,
	                                                           const IRenderAdapter* renderer)
	{
		const auto* runtime = world.GetRegistry().try_get<ECS::MeshRenderRuntimeComponent>(entity);
		if (runtime == nullptr) return nullptr;

		if (const auto rendererIt = runtime->bindingsByRenderer.find(GetRendererBindingKey(renderer));
		    rendererIt != runtime->bindingsByRenderer.end())
			return &rendererIt->second;

		if (const auto fallbackIt = runtime->bindingsByRenderer.find(0); fallbackIt != runtime->bindingsByRenderer.end())
			return &fallbackIt->second;

		return nullptr;
	}

	void ClearMeshRenderRuntimeBinding(ECS::World& world, ECS::Entity entity)
	{
		if (world.HasComponent<ECS::MeshRenderRuntimeComponent>(entity))
			world.RemoveComponent<ECS::MeshRenderRuntimeComponent>(entity);
	}

} // namespace NeneEngine
