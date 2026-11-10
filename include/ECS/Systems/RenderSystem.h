// RenderSystem.h

#pragma once

#include "Core/JobSystem.h"
#include "ECS/Entity.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <EASTL/vector.h>
#include <cstdint>

namespace NeneEngine::ECS
{

	class World;
	struct ColliderComponent;
	struct TransformComponent;

	class RenderSystem final
	{
	  public:
		explicit RenderSystem(IRenderAdapter* adapter, Entity cameraEntity = NullEntity,
		                      const SceneAssetResolver* assets = nullptr, JobSystem* jobs = nullptr)
		    : m_renderer(adapter), m_assets(assets ? *assets : SceneAssetResolver{}), m_jobs(jobs),
		      m_cameraEntity(cameraEntity)
		{
		}

		void Render(World& world);
		void SetCameraEntity(Entity cameraEntity) { m_cameraEntity = cameraEntity; }

	  private:
		struct DebugBounds
		{
			glm::vec3 min{0.0f};
			glm::vec3 max{0.0f};
			glm::vec4 color{1.0f};
		};

		struct DrawRecord
		{
			Entity entity = NullEntity;
			uint32_t transformIndex = 0;
			PrimitiveType primitiveType = PrimitiveType::Triangle;
			glm::vec4 tint{1.0f};
			MeshId meshId{};
			ShaderId shaderId{};
			TextureId textureId{};
		};

		void BuildTransformSnapshot(World& world);
		void BuildHierarchyLevels();
		void ComputeWorldMatrices();
		void CollectDrawRecords(World& world);
		void BuildRenderItems(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
		                      const glm::mat4& viewProjectionMatrix);
		void SubmitRenderItems();
		void RenderColliderBounds(World& world, const glm::mat4& viewProjectionMatrix);
		void ForEachRange(const char* name, uint32_t count, const JobSystem::RangeFunction& function);
		[[nodiscard]] uint32_t FindTransformIndex(Entity entity) const;

		IRenderAdapter* m_renderer;
		SceneAssetResolver m_assets;
		JobSystem* m_jobs = nullptr;
		Entity m_cameraEntity = NullEntity;
		bool m_useJobs = true;

		eastl::vector<Entity> m_entities;
		eastl::vector<const TransformComponent*> m_transforms;
		eastl::vector<uint32_t> m_parents;
		eastl::vector<uint32_t> m_indexByEntity;
		eastl::vector<uint32_t> m_depths;
		eastl::vector<uint32_t> m_walkPath;
		eastl::vector<uint32_t> m_levelOffsets;
		eastl::vector<uint32_t> m_levelCursors;
		eastl::vector<uint32_t> m_levelOrder;
		eastl::vector<glm::mat4> m_worldMatrices;
		eastl::vector<DrawRecord> m_drawRecords;
		eastl::vector<RenderItem> m_items;
		eastl::vector<const ColliderComponent*> m_colliders;
		eastl::vector<uint32_t> m_colliderTransforms;
		eastl::vector<DebugBounds> m_colliderBounds;
	};

} // namespace NeneEngine::ECS
