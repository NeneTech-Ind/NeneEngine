// MeshRenderRuntimeComponent.cpp

#include "ECS/Components/MeshRenderRuntimeComponent.h"

#include "ECS/World.h"
#include "Graphics/Backend/IRenderAdapter.h"

namespace NeneEngine::ECS
{
	namespace
	{
		uintptr_t GetRendererBindingKey(const IRenderAdapter* renderer)
		{
			return renderer != nullptr ? renderer->GetResourceDomainKey() : 0;
		}
	} // namespace

	void BindMeshRenderRuntime(World& world, Entity entity, const MeshRenderRuntimeBinding& binding,
	                           const IRenderAdapter* renderer)
	{
		auto* runtime = world.GetComponent<MeshRenderRuntimeComponent>(entity);
		if (runtime == nullptr) runtime = &world.AddComponent<MeshRenderRuntimeComponent>(entity);

		runtime->bindingsByRenderer[GetRendererBindingKey(renderer)] = binding;
	}

	const MeshRenderRuntimeBinding* GetMeshRenderRuntimeBinding(const World& world, Entity entity,
	                                                            const IRenderAdapter* renderer)
	{
		const auto* runtime = world.GetRegistry().try_get<MeshRenderRuntimeComponent>(entity);
		if (runtime == nullptr) return nullptr;

		if (const auto rendererIt = runtime->bindingsByRenderer.find(GetRendererBindingKey(renderer));
		    rendererIt != runtime->bindingsByRenderer.end())
			return &rendererIt->second;

		if (const auto fallbackIt = runtime->bindingsByRenderer.find(0);
		    fallbackIt != runtime->bindingsByRenderer.end())
			return &fallbackIt->second;

		return nullptr;
	}

	void ClearMeshRenderRuntimeBinding(World& world, Entity entity)
	{
		if (world.HasComponent<MeshRenderRuntimeComponent>(entity))
			world.RemoveComponent<MeshRenderRuntimeComponent>(entity);
	}

} // namespace NeneEngine::ECS
