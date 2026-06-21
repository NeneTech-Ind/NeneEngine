// RenderSystem.h

#pragma once

#include "ECS/Entity.h"
#include "ECS/Systems/ISystem.h"
#include "Graphics/Backend/IRenderAdapter.h"

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
		IRenderAdapter* m_renderer;
		Entity m_cameraEntity = NullEntity;
	};

} // namespace NeneEngine::ECS
