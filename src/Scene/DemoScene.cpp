// DemoScene.cpp

#include "Scene/DemoScene.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PlayerControllerComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Scene/SceneConfig.h"
#include "Scene/SceneSerializer.h"

#include <exception>

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

		// The built-in Cube primitive spans -0.4..0.4 on every axis.
		constexpr glm::vec3 kCubePrimitiveHalfExtents{0.4f, 0.4f, 0.4f};

		ECS::Entity CreatePhysicsCube(ECS::World& world, std::string_view name, const glm::vec3& position,
		                              const glm::vec3& scale, const glm::vec4& tint, bool isDynamic)
		{
			const ECS::Entity entity = CreatePrimitiveEntity(world, name, PrimitiveType::Cube, position, scale, tint);

			auto& collider = world.AddComponent<ECS::ColliderComponent>(entity);
			collider.type = ECS::ColliderType::Box;
			collider.halfExtents = kCubePrimitiveHalfExtents;

			if (isDynamic) world.AddComponent<ECS::RigidbodyComponent>(entity);
			return entity;
		}

		void CreatePhysicsDemo(ECS::World& world)
		{
			// Static colliders (no Rigidbody): the ground plane and an obstacle.
			CreatePhysicsCube(world, "PhysicsGround", {0.0f, -1.8f, -2.0f}, {20.0f, 1.0f, 15.0f},
			                  {0.45f, 0.45f, 0.5f, 1.0f}, false);
			CreatePhysicsCube(world, "PhysicsObstacle", {-3.5f, -1.0f, -2.0f}, {1.5f, 1.0f, 1.5f},
			                  {0.3f, 0.45f, 0.9f, 1.0f}, false);

			// Dynamic bodies fall under gravity and settle on the ground.
			CreatePhysicsCube(world, "PhysicsCubeA", {3.0f, 2.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.55f, 0.2f, 1.0f},
			                  true);

			const ECS::Entity tiltedCube = CreatePhysicsCube(world, "PhysicsCubeB", {3.3f, 3.5f, -1.0f},
			                                                 {1.0f, 1.0f, 1.0f}, {0.9f, 0.3f, 0.6f, 1.0f}, true);
			world.GetComponent<ECS::TransformComponent>(tiltedCube)->rotation =
			    glm::angleAxis(0.4f, glm::normalize(glm::vec3{1.0f, 0.0f, 1.0f}));

			const ECS::Entity heavyCube = CreatePhysicsCube(world, "PhysicsCubeC", {4.2f, 5.0f, -1.2f},
			                                                {1.3f, 1.3f, 1.3f}, {0.6f, 0.9f, 0.3f, 1.0f}, true);
			world.GetComponent<ECS::RigidbodyComponent>(heavyCube)->mass = 3.0f;

			// Player-controlled body: arrows move it, Enter jumps.
			const ECS::Entity player = CreatePhysicsCube(world, "PlayerCube", {0.0f, 0.5f, -2.0f}, {1.0f, 1.0f, 1.0f},
			                                             {0.2f, 0.9f, 0.95f, 1.0f}, true);
			world.GetComponent<ECS::RigidbodyComponent>(player)->freezeRotation = true;
			world.AddComponent<ECS::PlayerControllerComponent>(player);
		}

		void SaveDefaultScene(ECS::World& world, uint32_t width, uint32_t height, const std::filesystem::path& scenePath)
		{
			Create(world, width, height);

			const std::filesystem::path parentPath = scenePath.parent_path();
			if (!parentPath.empty()) std::filesystem::create_directories(parentPath);

			SceneSerializer::SaveToFile(world, scenePath);
		}

	} // namespace

	std::filesystem::path DefaultScenePath()
	{
		return std::filesystem::path{"assets"} / "scenes" / "demo_scene.json";
	}

	std::filesystem::path DefaultSceneConfigPath()
	{
		return std::filesystem::path{"assets"} / "scenes" / "demo_scene.config.json";
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

		CreatePhysicsDemo(world);
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
			try
			{
				SceneSerializer::LoadFromFile(effectiveScenePath, world);
			}
			catch (const std::exception& ex)
			{
				NENE_LOG_WARN("Demo scene '{}' failed to load: {}. Recreating default scene",
				              effectiveScenePath.string(), ex.what());
				SaveDefaultScene(world, width, height, effectiveScenePath);
			}
		}
		else
		{
			SaveDefaultScene(world, width, height, effectiveScenePath);
		}

		ApplySceneConfig(world, LoadSceneConfig(effectiveSceneConfigPath));
	}

} // namespace NeneEngine::DemoScene
