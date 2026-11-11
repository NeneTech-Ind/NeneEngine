// RenderSystem.cpp

#include "ECS/Systems/RenderSystem.h"
#include "Core/NeneLogger.h"
#include "Core/Profiler.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/ContactStateComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/DebugDrawSettings.h"
#include "ECS/RenderSettings.h"
#include "ECS/World.h"

#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr uint32_t kNoIndex = (std::numeric_limits<uint32_t>::max)();
		constexpr uint32_t kUnknownDepth = kNoIndex;
		constexpr uint32_t kVisitingDepth = kNoIndex - 1;
		constexpr uint32_t kMinJobRange = 256;
		constexpr float kStatsSmoothing = 0.1f;

		uint32_t ToEntitySlot(Entity entity)
		{
			return static_cast<uint32_t>(entt::to_entity(entity));
		}

		uint32_t ToEntityId(Entity entity)
		{
			return static_cast<uint32_t>(entt::to_integral(entity));
		}

		constexpr glm::vec4 kStaticColliderColor{0.3f, 0.6f, 1.0f, 1.0f};
		constexpr glm::vec4 kDynamicColliderColor{0.2f, 1.0f, 0.2f, 1.0f};
		constexpr glm::vec4 kTouchingColliderColor{1.0f, 0.15f, 0.15f, 1.0f};

		// World-space AABB enclosing the collider after the entity's rotation and scale are applied.
		void ComputeColliderBounds(const glm::mat4& worldMatrix, const ColliderComponent& collider, glm::vec3& min,
		                           glm::vec3& max)
		{
			const glm::vec3 center = glm::vec3(worldMatrix * glm::vec4(collider.offset, 1.0f));
			const glm::vec3 basisX = glm::vec3(worldMatrix[0]);
			const glm::vec3 basisY = glm::vec3(worldMatrix[1]);
			const glm::vec3 basisZ = glm::vec3(worldMatrix[2]);

			glm::vec3 extent{0.0f};
			if (collider.type == ColliderType::Sphere)
			{
				const float maxScaleAxis = (std::max)({glm::length(basisX), glm::length(basisY), glm::length(basisZ)});
				extent = glm::vec3(collider.radius * maxScaleAxis);
			}
			else
			{
				const glm::mat3 absoluteBasis{glm::abs(basisX), glm::abs(basisY), glm::abs(basisZ)};
				extent = absoluteBasis * collider.halfExtents;
			}

			min = center - extent;
			max = center + extent;
		}
	} // namespace

	void RenderSystem::Render(World& world)
	{
		NENE_PROFILE_SCOPE("RenderSystem");
		const auto startTime = std::chrono::steady_clock::now();

		if (m_renderer == nullptr)
		{
			NENE_LOG_WARN("RenderSystem: render adapter is null");
			return;
		}

		auto& registry = world.GetRegistry();
		const auto* settings = registry.ctx().find<RenderSettings>();
		m_useJobs = m_jobs != nullptr && (settings == nullptr || settings->useJobSystem);

		const auto cameraView = registry.view<const TransformComponent, const CameraComponent>();

		Entity activeCameraEntity = NullEntity;
		const CameraComponent* activeCamera = nullptr;

		for (auto entity : cameraView)
		{
			if (m_cameraEntity != NullEntity && entity != m_cameraEntity) continue;

			const auto& camera = cameraView.get<CameraComponent>(entity);
			if (m_cameraEntity == NullEntity && !camera.isPrimary) continue;

			activeCameraEntity = entity;
			activeCamera = &camera;
			break;
		}

		if (activeCameraEntity == NullEntity || activeCamera == nullptr)
		{
			NENE_LOG_WARN("RenderSystem: no primary camera found");
			return;
		}

		BuildTransformSnapshot(world);
		ComputeWorldMatrices();

		const glm::mat4& cameraWorldMatrix = m_worldMatrices[FindTransformIndex(activeCameraEntity)];
		const glm::vec3 cameraPosition = glm::vec3(cameraWorldMatrix[3]);
		// CameraComponent stores world-space orientation vectors updated by CameraControllerSystem,
		// so applying the world transform again would rotate the view direction twice.
		const glm::vec3 cameraForward = glm::normalize(activeCamera->forward);
		const glm::vec3 cameraUp = glm::normalize(activeCamera->up);
		const glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp);
		const glm::mat4 projectionMatrix = activeCamera->GetProjectionMatrix();
		const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

		CollectDrawRecords(world);
		BuildRenderItems(viewMatrix, projectionMatrix, viewProjectionMatrix);
		SubmitRenderItems();

		const auto* debugDraw = registry.ctx().find<DebugDrawSettings>();
		if (debugDraw != nullptr && debugDraw->drawColliders) RenderColliderBounds(world, viewProjectionMatrix);

		const float elapsedMs =
		    std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
		auto& stats = registry.ctx().emplace<RenderStats>();
		stats.preparationMs = stats.usedJobSystem == m_useJobs && stats.preparationMs > 0.0f
		                          ? stats.preparationMs + (elapsedMs - stats.preparationMs) * kStatsSmoothing
		                          : elapsedMs;
		stats.transformCount = static_cast<uint32_t>(m_entities.size());
		stats.hierarchyDepth = static_cast<uint32_t>(m_levelOffsets.size() - 1);
		stats.itemCount = static_cast<uint32_t>(m_items.size());
		stats.usedJobSystem = m_useJobs;
	}

	void RenderSystem::BuildTransformSnapshot(World& world)
	{
		NENE_PROFILE_SCOPE("Render Snapshot");
		auto& registry = world.GetRegistry();

		m_entities.clear();
		m_transforms.clear();
		uint32_t maxSlot = 0;
		for (const auto [entity, transform] : registry.view<const TransformComponent>().each())
		{
			m_entities.push_back(entity);
			m_transforms.push_back(&transform);
			maxSlot = (std::max)(maxSlot, ToEntitySlot(entity));
		}

		const auto count = static_cast<uint32_t>(m_entities.size());
		m_indexByEntity.assign(count == 0 ? 0 : maxSlot + 1, kNoIndex);
		for (uint32_t index = 0; index < count; ++index) m_indexByEntity[ToEntitySlot(m_entities[index])] = index;

		m_parents.assign(count, kNoIndex);
		for (const auto [entity, transform, hierarchy] :
		     registry.view<const TransformComponent, const HierarchyComponent>().each())
			m_parents[FindTransformIndex(entity)] = FindTransformIndex(hierarchy.parent);

		BuildHierarchyLevels();
	}

	void RenderSystem::BuildHierarchyLevels()
	{
		const auto count = static_cast<uint32_t>(m_entities.size());
		m_depths.assign(count, kUnknownDepth);
		uint32_t maxDepth = 0;

		for (uint32_t start = 0; start < count; ++start)
		{
			m_walkPath.clear();
			uint32_t current = start;
			while (m_depths[current] == kUnknownDepth)
			{
				m_depths[current] = kVisitingDepth;
				m_walkPath.push_back(current);
				if (m_parents[current] == kNoIndex) break;
				current = m_parents[current];
			}

			if (m_depths[current] == kVisitingDepth && m_parents[current] != kNoIndex)
			{
				NENE_LOG_WARN("RenderSystem: hierarchy cycle detected for entity {}", ToEntityId(m_entities[current]));
				m_parents[current] = kNoIndex;
				m_depths[current] = 0;
			}

			for (auto it = m_walkPath.rbegin(); it != m_walkPath.rend(); ++it)
			{
				const uint32_t parent = m_parents[*it];
				m_depths[*it] = parent == kNoIndex ? 0 : m_depths[parent] + 1;
				maxDepth = (std::max)(maxDepth, m_depths[*it]);
			}
		}

		const uint32_t levelCount = count == 0 ? 0 : maxDepth + 1;
		m_levelOffsets.assign(levelCount + 1, 0);
		for (uint32_t index = 0; index < count; ++index) ++m_levelOffsets[m_depths[index] + 1];
		for (uint32_t level = 0; level < levelCount; ++level) m_levelOffsets[level + 1] += m_levelOffsets[level];

		m_levelCursors.assign(m_levelOffsets.begin(), m_levelOffsets.end() - 1);
		m_levelOrder.resize(count);
		for (uint32_t index = 0; index < count; ++index) m_levelOrder[m_levelCursors[m_depths[index]]++] = index;
	}

	void RenderSystem::ComputeWorldMatrices()
	{
		NENE_PROFILE_SCOPE("Render World Matrices");
		m_worldMatrices.resize(m_entities.size());

		const auto levelCount = static_cast<uint32_t>(m_levelOffsets.size() - 1);
		const auto computeLevel = [this](uint32_t level)
		{
			return [this, levelBegin = m_levelOffsets[level]](uint32_t begin, uint32_t end)
			{
				for (uint32_t position = levelBegin + begin; position < levelBegin + end; ++position)
				{
					const uint32_t index = m_levelOrder[position];
					const glm::mat4 localMatrix = m_transforms[index]->GetModelMatrix();
					const uint32_t parent = m_parents[index];
					m_worldMatrices[index] = parent == kNoIndex ? localMatrix : m_worldMatrices[parent] * localMatrix;
				}
			};
		};
		const auto levelSize = [this](uint32_t level) { return m_levelOffsets[level + 1] - m_levelOffsets[level]; };

		if (m_useJobs && m_entities.size() > kMinJobRange)
		{
			JobHandle first;
			JobHandle last;
			for (uint32_t level = 0; level < levelCount; ++level)
			{
				const JobHandle job =
				    m_jobs->CreateParallelFor("World Matrices", levelSize(level), kMinJobRange, computeLevel(level));
				if (last.IsValid())
					m_jobs->AddDependency(job, last);
				else
					first = job;
				last = job;
			}
			if (m_jobs->Submit(first))
			{
				m_jobs->Wait(last);
				return;
			}
		}

		for (uint32_t level = 0; level < levelCount; ++level) computeLevel(level)(0, levelSize(level));
	}

	void RenderSystem::CollectDrawRecords(World& world)
	{
		NENE_PROFILE_SCOPE("Render Collect");
		m_drawRecords.clear();

		auto view = world.GetRegistry().view<const TransformComponent, const MeshRendererComponent>();
		for (auto entity : view)
		{
			const auto& meshRenderer = view.get<MeshRendererComponent>(entity);
			if (!meshRenderer.visible) continue;

			DrawRecord record{entity, FindTransformIndex(entity), meshRenderer.primitiveType, meshRenderer.tint};
			if (!meshRenderer.meshPath.empty() || !meshRenderer.texturePath.empty() || !meshRenderer.shaderPath.empty())
			{
				const auto binding =
				    m_assets.Resolve(meshRenderer.meshPath, meshRenderer.meshPart, meshRenderer.texturePath,
				                     meshRenderer.shaderPath, meshRenderer.primitiveType, *m_renderer);
				if (!binding.meshId.IsValid() && !meshRenderer.meshPath.empty()) continue;
				record.meshId = binding.meshId;
				record.shaderId = binding.shaderId;
				record.textureId = binding.textureId;
			}

			m_drawRecords.push_back(record);
		}
	}

	void RenderSystem::BuildRenderItems(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
	                                    const glm::mat4& viewProjectionMatrix)
	{
		NENE_PROFILE_SCOPE("Render Build Items");
		m_items.resize(m_drawRecords.size());

		ForEachRange("Render Items", static_cast<uint32_t>(m_drawRecords.size()),
		             [&](uint32_t begin, uint32_t end)
		             {
			             for (uint32_t index = begin; index < end; ++index)
			             {
				             const DrawRecord& record = m_drawRecords[index];
				             RenderItem& item = m_items[index];
				             item.modelMatrix = m_worldMatrices[record.transformIndex];
				             item.viewMatrix = viewMatrix;
				             item.projectionMatrix = projectionMatrix;
				             item.viewProjectionMatrix = viewProjectionMatrix;
				             item.modelViewProjectionMatrix = viewProjectionMatrix * item.modelMatrix;
				             item.primitiveType = record.primitiveType;
				             item.tint = record.tint;
				             item.meshId = record.meshId;
				             item.materialId = {};
				             item.shaderId = record.shaderId;
				             item.textureId = record.textureId;
			             }
		             });
	}

	void RenderSystem::SubmitRenderItems()
	{
		NENE_PROFILE_SCOPE("Render Submit");
		NENE_LOG_DEBUG("RenderSystem: starting render pass");

		for (uint32_t index = 0; index < m_items.size(); ++index)
		{
			const RenderItem& item = m_items[index];
			m_renderer->SubmitRenderItem(item);

			NENE_LOG_DEBUG("RenderSystem: submitted entity {} | primitive={} | mesh={} | material={} | shader={} | "
			               "texture={}",
			               ToEntityId(m_drawRecords[index].entity), static_cast<int>(item.primitiveType),
			               item.meshId.value, item.materialId.value, item.shaderId.value, item.textureId.value);
		}
	}

	void RenderSystem::RenderColliderBounds(World& world, const glm::mat4& viewProjectionMatrix)
	{
		NENE_PROFILE_SCOPE("Render Collider Bounds");
		m_colliders.clear();
		m_colliderTransforms.clear();
		m_colliderBounds.clear();

		auto view = world.GetRegistry().view<const TransformComponent, const ColliderComponent>();
		for (auto [entity, transform, collider] : view.each())
		{
			const auto* contactState = world.GetComponent<ContactStateComponent>(entity);
			glm::vec4 color =
			    world.HasComponent<RigidbodyComponent>(entity) ? kDynamicColliderColor : kStaticColliderColor;
			if (contactState != nullptr && contactState->contactCount > 0) color = kTouchingColliderColor;

			m_colliders.push_back(&collider);
			m_colliderTransforms.push_back(FindTransformIndex(entity));
			m_colliderBounds.push_back({.color = color});
		}

		ForEachRange("Collider Bounds", static_cast<uint32_t>(m_colliders.size()),
		             [this](uint32_t begin, uint32_t end)
		             {
			             for (uint32_t index = begin; index < end; ++index)
			             {
				             DebugBounds& bounds = m_colliderBounds[index];
				             ComputeColliderBounds(m_worldMatrices[m_colliderTransforms[index]], *m_colliders[index],
				                                   bounds.min, bounds.max);
			             }
		             });

		for (const DebugBounds& bounds : m_colliderBounds)
			m_renderer->DrawDebugAABB(bounds.min, bounds.max, bounds.color, viewProjectionMatrix);
	}

	void RenderSystem::ForEachRange(const char* name, uint32_t count, const JobSystem::RangeFunction& function)
	{
		if (count == 0) return;

		if (m_useJobs && count > kMinJobRange)
		{
			const JobHandle job = m_jobs->ParallelFor(name, count, kMinJobRange, function);
			if (job.IsValid())
			{
				m_jobs->Wait(job);
				return;
			}
		}

		function(0, count);
	}

	uint32_t RenderSystem::FindTransformIndex(Entity entity) const
	{
		if (entity == NullEntity) return kNoIndex;

		const uint32_t slot = ToEntitySlot(entity);
		if (slot >= m_indexByEntity.size()) return kNoIndex;

		const uint32_t index = m_indexByEntity[slot];
		return index != kNoIndex && m_entities[index] == entity ? index : kNoIndex;
	}

} // namespace NeneEngine::ECS
