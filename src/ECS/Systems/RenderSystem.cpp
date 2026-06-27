// RenderSystem.cpp

#include "ECS/Systems/RenderSystem.h"
#include "Core/CustomLogger.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Graphics/Runtime/MeshRenderBinding.h"

#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <unordered_set>

namespace NeneEngine::ECS
{
	namespace
	{
		uint32_t ToEntityId(Entity entity)
		{
			return static_cast<uint32_t>(entt::to_integral(entity));
		}

		glm::mat4 ComputeWorldMatrix(World& world, Entity entity, std::unordered_map<uint32_t, glm::mat4>& cache,
		                             std::unordered_set<uint32_t>& recursionStack)
		{
			// Hierarchy traversal is cached per render pass and guarded against accidental parent cycles.
			const uint32_t entityId = ToEntityId(entity);
			if (const auto cached = cache.find(entityId); cached != cache.end()) return cached->second;

			const auto* transform = world.GetComponent<TransformComponent>(entity);
			if (transform == nullptr) return glm::mat4(1.0f);

			const glm::mat4 localMatrix = transform->GetModelMatrix();
			const auto* hierarchy = world.GetComponent<HierarchyComponent>(entity);
			if (hierarchy == nullptr || hierarchy->parent == NullEntity)
			{
				cache[entityId] = localMatrix;
				return localMatrix;
			}

			if (!recursionStack.insert(entityId).second)
			{
				NENE_LOG_WARN("RenderSystem: hierarchy cycle detected for entity {}", entityId);
				cache[entityId] = localMatrix;
				return localMatrix;
			}

			const glm::mat4 parentWorldMatrix = ComputeWorldMatrix(world, hierarchy->parent, cache, recursionStack);
			recursionStack.erase(entityId);

			const glm::mat4 worldMatrix = parentWorldMatrix * localMatrix;
			cache[entityId] = worldMatrix;
			return worldMatrix;
		}
	} // namespace

	void RenderSystem::Update(World& /*world*/, float /*deltaTime*/) {}

	void RenderSystem::Render(World& world)
	{
		if (m_renderer == nullptr)
		{
			NENE_LOG_WARN("RenderSystem: render adapter is null");
			return;
		}

		const auto cameraView = world.GetRegistry().view<const TransformComponent, const CameraComponent>();

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

		std::unordered_map<uint32_t, glm::mat4> worldMatrixCache;
		std::unordered_set<uint32_t> recursionStack;

		const glm::mat4 cameraWorldMatrix =
		    ComputeWorldMatrix(world, activeCameraEntity, worldMatrixCache, recursionStack);
		const glm::vec3 cameraPosition = glm::vec3(cameraWorldMatrix[3]);
		// CameraComponent stores world-space orientation vectors updated by CameraControllerSystem,
		// so applying the world transform again would rotate the view direction twice.
		const glm::vec3 cameraForward = glm::normalize(activeCamera->forward);
		const glm::vec3 cameraUp = glm::normalize(activeCamera->up);
		const glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp);
		const glm::mat4 projectionMatrix = activeCamera->GetProjectionMatrix();
		const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

		auto view = world.GetRegistry().view<const TransformComponent, const MeshRendererComponent>();

		NENE_LOG_DEBUG("RenderSystem: starting render pass");

		for (auto entity : view)
		{
			const auto& transform = view.get<TransformComponent>(entity);
			const auto& meshRenderer = view.get<MeshRendererComponent>(entity);

			if (!meshRenderer.visible) continue;

			RenderItem item{};
			item.modelMatrix = ComputeWorldMatrix(world, entity, worldMatrixCache, recursionStack);
			item.viewMatrix = viewMatrix;
			item.projectionMatrix = projectionMatrix;
			item.viewProjectionMatrix = viewProjectionMatrix;
			item.modelViewProjectionMatrix = viewProjectionMatrix * item.modelMatrix;
			item.primitiveType = meshRenderer.primitiveType;
			item.tint = meshRenderer.tint;

			if (const auto* renderRuntime = GetMeshRenderRuntimeBinding(world, entity, m_renderer);
			    renderRuntime != nullptr)
			{
				item.meshId = renderRuntime->meshId;
				item.materialId = renderRuntime->materialId;
				item.shaderId = renderRuntime->shaderId;
				item.textureId = renderRuntime->textureId;
			}

			m_renderer->SubmitRenderItem(item);

			NENE_LOG_DEBUG("RenderSystem: submitted entity {} | primitive={} | mesh={} | material={} | shader={} | "
			               "texture={}",
			               static_cast<uint32_t>(entt::to_integral(entity)), static_cast<int>(item.primitiveType),
			               item.meshId.value, item.materialId.value, item.shaderId.value, item.textureId.value);
		}
	}

} // namespace NeneEngine::ECS
