// RenderSystem.h

#pragma once

#include "ECS/Entity.h"
#include "ECS/Systems/ISystem.h"
#include "Graphics/Backend/IRenderAdapter.h"

#include <cstdint>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>

namespace NeneEngine::ECS
{

	class RenderSystem final : public ISystem
	{
	  public:
		explicit RenderSystem(IRenderAdapter* adapter, Entity cameraEntity = NullEntity)
		    : m_renderer(adapter), m_cameraEntity(cameraEntity)
		{
		}

		void Update(World& world, float deltaTime) override;

		void Render(World& world) override;
		void SetCameraEntity(Entity cameraEntity) { m_cameraEntity = cameraEntity; }

	  private:
		void RenderColliderBounds(World& world, const glm::mat4& viewProjectionMatrix,
		                          eastl::unordered_map<uint32_t, glm::mat4>& worldMatrixCache,
		                          eastl::unordered_set<uint32_t>& recursionStack);

		IRenderAdapter* m_renderer;
		Entity m_cameraEntity = NullEntity;
	};

} // namespace NeneEngine::ECS
