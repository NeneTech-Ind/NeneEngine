// DemoScene.cpp

#include "Scene/DemoScene.h"

#include "Core/PathResolver.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Scene/SceneConfig.h"
#include "Scene/SceneSerializer.h"

namespace NeneEngine::DemoScene
{
	namespace
	{
		ECS::Entity CreatePrimitiveEntity(ECS::World& world, std::string_view name, PrimitiveType primitiveType,
		                                  const glm::vec3& position, const glm::vec3& scale, const glm::vec4& tint)
		{
			const ECS::Entity entity = world.CreateEntity(std::string(name));
			auto& transform = world.AddComponent<ECS::TransformComponent>(entity);
			transform.position = position;
			transform.scale = scale;

			auto& renderer = world.AddComponent<ECS::MeshRendererComponent>(entity);
			renderer.primitiveType = primitiveType;
			renderer.tint = tint;

			return entity;
		}

	} // namespace

	std::filesystem::path DefaultScenePath()
	{
		return std::filesystem::path{"assets"} / "scenes" / "test_scene.json";
	}

	std::filesystem::path DefaultSceneConfigPath()
	{
		return std::filesystem::path{"assets"} / "scenes" / "test_scene.config.json";
	}

	void Create(ECS::World& world, uint32_t width, uint32_t height)
	{
		world.GetRegistry().clear();

		const ECS::Entity cameraEntity = world.CreateEntity("MainCamera");
		auto& cameraTransform = world.AddComponent<ECS::TransformComponent>(cameraEntity);
		cameraTransform.position = {0.0f, 0.0f, 8.0f};

		auto& camera = world.AddComponent<ECS::CameraComponent>(cameraEntity);
		camera.aspectRatio = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
		camera.fovDegrees = 60.0f;
		camera.nearPlane = 0.1f;
		camera.farPlane = 100.0f;
		camera.isPrimary = true;

		auto& cameraController = world.AddComponent<ECS::CameraControllerComponent>(cameraEntity);
		cameraController.moveSpeed = 4.0f;

		CreatePrimitiveEntity(world, "SceneLine", PrimitiveType::Line, {-3.5f, 1.8f, 0.0f}, {2.5f, 1.0f, 1.0f},
		                      {1.0f, 0.35f, 0.35f, 1.0f});

		const ECS::Entity controllableTriangle =
		    CreatePrimitiveEntity(world, "SceneTriangle", PrimitiveType::Triangle, {-1.2f, -1.4f, 0.0f},
		                          {1.4f, 1.4f, 1.0f}, {0.35f, 1.0f, 0.45f, 1.0f});

		auto& primitiveControl = world.AddComponent<ECS::PrimitiveControlComponent>(controllableTriangle);
		primitiveControl.currentScaleLevel = 0;
		primitiveControl.targetScale = {1.0f, 1.0f, 1.0f};

		const ECS::Entity movingQuad =
		    CreatePrimitiveEntity(world, "SceneQuad", PrimitiveType::Quad, {1.4f, 1.0f, 0.0f}, {2.1f, 1.2f, 1.0f},
		                          {0.25f, 0.75f, 1.0f, 1.0f});

		auto& movement = world.AddComponent<ECS::MovementComponent>(movingQuad);
		movement.origin = {1.4f, 1.0f, 0.0f};
		movement.oscillationAxis = {1.0f, 0.0f, 0.0f};
		movement.oscillationAmplitude = 1.25f;
		movement.oscillationSpeed = 1.5f;
		movement.useOscillation = true;

		const ECS::Entity sceneCube =
		    CreatePrimitiveEntity(world, "SceneCube", PrimitiveType::Cube, {1.5f, -1.2f, 0.0f}, {0.9f, 0.9f, 0.9f},
		                          {1.0f, 0.85f, 0.3f, 1.0f});

		auto& quadHierarchy = world.AddComponent<ECS::HierarchyComponent>(movingQuad);
		quadHierarchy.children.push_back(sceneCube);

		auto& cubeHierarchy = world.AddComponent<ECS::HierarchyComponent>(sceneCube);
		cubeHierarchy.parent = movingQuad;
	}

	void LoadOrCreate(ECS::World& world, uint32_t width, uint32_t height, const std::filesystem::path& scenePath,
	                  const std::filesystem::path& sceneConfigPath)
	{
		const std::filesystem::path resolvedScenePath =
		    scenePath.is_absolute() ? scenePath : ResolveFromExecutionRoots(scenePath, true);
		const std::filesystem::path resolvedSceneConfigPath =
		    sceneConfigPath.is_absolute() ? sceneConfigPath : ResolveFromExecutionRoots(sceneConfigPath);

		const std::filesystem::path effectiveScenePath = resolvedScenePath.empty() ? scenePath : resolvedScenePath;
		const std::filesystem::path effectiveSceneConfigPath =
		    resolvedSceneConfigPath.empty() ? sceneConfigPath : resolvedSceneConfigPath;

		if (std::filesystem::exists(effectiveScenePath))
		{
			SceneSerializer::LoadFromFile(effectiveScenePath, world);
		}
		else
		{
			Create(world, width, height);

			const std::filesystem::path parentPath = effectiveScenePath.parent_path();
			if (!parentPath.empty()) std::filesystem::create_directories(parentPath);

			SceneSerializer::SaveToFile(world, effectiveScenePath);
		}

		ApplySceneConfig(world, LoadSceneConfig(effectiveSceneConfigPath));
	}

} // namespace NeneEngine::DemoScene
