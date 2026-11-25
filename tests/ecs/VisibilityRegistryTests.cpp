// VisibilityRegistryTests.cpp

#include "ECS/Components/CameraComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/VisibilityRegistry.h"
#include "ECS/World.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Runtime/GeometryBounds.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>

namespace
{
	using namespace NeneEngine::ECS;

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << message << '\n';
		std::exit(EXIT_FAILURE);
	}

	void TestLocalGeometryBounds()
	{
		using namespace NeneEngine;
		const auto checkPrimitive = [](PrimitiveType primitive, const glm::vec3& extent)
		{
			const auto bounds = GetPrimitiveLocalBounds(primitive);
			Check(bounds && bounds->min == -extent && bounds->max == extent, "Primitive bounds must match its geometry");
		};
		checkPrimitive(PrimitiveType::Line, {0.5f, 0.0f, 0.0f});
		checkPrimitive(PrimitiveType::DebugLine, {0.5f, 0.0f, 0.0f});
		checkPrimitive(PrimitiveType::Triangle, {0.5f, 0.5f, 0.0f});
		checkPrimitive(PrimitiveType::Quad, {0.5f, 0.5f, 0.0f});
		checkPrimitive(PrimitiveType::Cube, glm::vec3(0.4f));
		Check(!GetPrimitiveLocalBounds(static_cast<PrimitiveType>(255)), "Unknown primitive must have unknown bounds");

		MeshData mesh;
		Check(!ComputeLocalBounds(mesh), "Empty mesh must have unknown bounds");
		mesh.vertices.push_back(Vertex{{2.0f, 4.0f, 1.0f}});
		const auto point = ComputeLocalBounds(mesh);
		Check(point && point->min == mesh.vertices.front().position && point->max == point->min,
		      "A single position must produce a valid point bound");
		mesh.vertices.push_back(Vertex{{3.0f, 2.0f, 1.0f}});
		const auto plane = ComputeLocalBounds(mesh);
		Check(plane && plane->min == glm::vec3(2.0f, 2.0f, 1.0f) && plane->max == glm::vec3(3.0f, 4.0f, 1.0f),
		      "Bounds must use per-axis extrema and preserve zero extent");
		mesh.vertices.back().position.x = std::numeric_limits<float>::quiet_NaN();
		Check(!ComputeLocalBounds(mesh), "Non-finite positions must have unknown bounds");
	}

	void TestImportedModelBounds()
	{
		using namespace NeneEngine;
		const auto model = LoadMeshPartsFromFile(NENE_TEST_FIXTURE_DIR "/translated-mesh-parts.gltf");
		Check(model.parts.size() == 2, "Translated model fixture must retain two parts");
		for (int index = 0; index < 2; ++index)
		{
			const auto bounds = model.GetLocalBounds(index);
			const auto fromVertices = ComputeLocalBounds(model.parts[index].data);
			Check(bounds && fromVertices && bounds->min == fromVertices->min && bounds->max == fromVertices->max,
			      "Selected part bounds must match its imported vertices");
			const glm::vec3 offset = bounds->min.x < 20.0f ? glm::vec3(10, 2, 3) : glm::vec3(25, 5, 8);
			Check(bounds->min == offset + glm::vec3(1, 1, 1) && bounds->max == offset + glm::vec3(3, 4, 1),
			      "Part bounds must include the baked node translation exactly once");
		}
		const auto merged = model.GetLocalBounds(-1);
		Check(merged && merged->min == glm::vec3(11, 3, 4) && merged->max == glm::vec3(28, 9, 9),
		      "Whole-model bounds must enclose both parts");
		Check(!model.GetLocalBounds(2), "An invalid part index must have unknown bounds");
	}

	bool HasBounds(const VisibilityRegistry& registry, Entity entity, const WorldAabb& expected)
	{
		const auto* bounds = registry.FindBounds(entity);
		return bounds != nullptr && bounds->has_value() && (*bounds)->min == expected.min &&
		       (*bounds)->max == expected.max;
	}

	bool HasUnknownBounds(const VisibilityRegistry& registry, Entity entity)
	{
		const auto* bounds = registry.FindBounds(entity);
		return bounds != nullptr && !bounds->has_value();
	}

	VisibilityCamera MakeCamera(const glm::vec3& worldPosition, float aspectRatio = 1.0f)
	{
		CameraComponent camera;
		camera.fovDegrees = 90.0f;
		camera.nearPlane = 1.0f;
		camera.farPlane = 10.0f;
		camera.aspectRatio = aspectRatio;
		return {camera.GetProjectionMatrix() * camera.GetViewMatrix(worldPosition), worldPosition, camera.forward,
		        camera.farPlane};
	}

	void CheckVisible(const eastl::vector<Entity>& actual, std::initializer_list<Entity> expected, const char* message)
	{
		Check(actual.size() == expected.size() && std::is_permutation(actual.begin(), actual.end(), expected.begin()),
		      message);
	}

	void TestCameraQueries()
	{
		const Entity left = entt::entt_traits<Entity>::construct(1, 0);
		const Entity right = entt::entt_traits<Entity>::construct(2, 0);
		const Entity pending = entt::entt_traits<Entity>::construct(3, 0);
		const Entity outsideNarrowCamera = entt::entt_traits<Entity>::construct(4, 0);
		const WorldAabb leftBounds{{-0.5f, -0.5f, -5.5f}, {0.5f, 0.5f, -4.5f}};
		const WorldAabb rightBounds{{19.5f, -0.5f, -5.5f}, {20.5f, 0.5f, -4.5f}};
		const VisibilityCamera leftCamera = MakeCamera({0.0f, 0.0f, 0.0f});
		const VisibilityCamera rightCamera = MakeCamera({20.0f, 0.0f, 0.0f}, 0.5f);
		VisibilityRegistry registry;
		Check(registry.Register(left, leftBounds), "Left entity must register");
		Check(registry.Register(right, rightBounds), "Right entity must register");
		Check(registry.Register(pending), "Pending entity must register");
		Check(registry.Register(outsideNarrowCamera, WorldAabb{{23.0f, -0.5f, -5.5f}, {24.0f, 0.5f, -4.5f}}),
		      "Entity outside the narrow camera must register");

		const VisibilityRegistry& scene = registry;
		eastl::vector<Entity> leftVisible{right};
		eastl::vector<Entity> rightVisible{left};
		scene.QueryVisible(leftCamera, leftVisible);
		scene.QueryVisible(rightCamera, rightVisible);
		CheckVisible(leftVisible, {left, pending}, "Left camera must replace its output with left and pending");
		CheckVisible(rightVisible, {right, pending}, "Right camera must use its own position and aspect ratio");
		scene.QueryVisible(leftCamera, leftVisible);
		CheckVisible(leftVisible, {left, pending}, "Repeating the left query must preserve its result");
		CheckVisible(rightVisible, {right, pending}, "Repeating the left query must preserve the right output");

		Check(registry.UpdateBounds(pending, rightBounds), "Pending bounds must become known");
		Check(registry.UpdateBounds(left, rightBounds), "Left entity must move to the right");
		scene.QueryVisible(leftCamera, leftVisible);
		scene.QueryVisible(rightCamera, rightVisible);
		CheckVisible(leftVisible, {}, "Updated bounds must leave the left view empty");
		CheckVisible(rightVisible, {left, right, pending}, "Updated bounds must be visible to the right camera");
		Check(registry.Remove(right), "Right entity must be removed");
		scene.QueryVisible(rightCamera, rightVisible);
		CheckVisible(rightVisible, {left, pending}, "Removed entities must disappear from queries");
	}

	void TestFrustumBoundaries()
	{
		const Entity entity = entt::entt_traits<Entity>::construct(1, 0);
		const VisibilityCamera camera = MakeCamera({0.0f, 0.0f, 0.0f});
		VisibilityRegistry registry;
		Check(registry.Register(entity), "Boundary test entity must register");
		eastl::vector<Entity> visible;
		const auto checkBounds = [&](const WorldAabb& bounds, bool expected, const char* message)
		{
			Check(registry.UpdateBounds(entity, bounds), "Boundary test entity must update");
			registry.QueryVisible(camera, visible);
			Check(expected ? visible.size() == 1 && visible.front() == entity : visible.empty(), message);
		};

		checkBounds({{2.0f, -0.5f, -5.0f}, {3.0f, 0.5f, -4.0f}}, true, "Fully inside must be visible");
		checkBounds({{4.5f, -0.5f, -5.0f}, {5.5f, 0.5f, -4.0f}}, true, "Side plane intersection must be visible");
		checkBounds({{5.0f, -0.5f, -5.0f}, {6.0f, 0.5f, -4.0f}}, true, "Side plane contact must be visible");
		checkBounds({{5.00001f, -0.5f, -5.0f}, {6.0f, 0.5f, -4.0f}}, true, "Roundoff at the side plane must be tolerated");
		checkBounds({{5.01f, -0.5f, -5.0f}, {6.0f, 0.5f, -4.0f}}, false, "Fully outside the side plane must be culled");
		checkBounds({{-0.2f, -0.2f, -1.0f}, {0.2f, 0.2f, -0.8f}}, true, "Near plane contact must be visible");
		checkBounds({{-0.2f, -0.2f, -0.99f}, {0.2f, 0.2f, -0.8f}}, false, "Fully before the near plane must be culled");
		checkBounds({{-0.2f, -0.2f, -10.2f}, {0.2f, 0.2f, -10.0f}}, true, "Far plane contact must be visible");
		checkBounds({{-0.2f, -0.2f, -10.2f}, {0.2f, 0.2f, -10.01f}}, false, "Fully beyond the far plane must be culled");
	}

	void TestFarPlanePrecision()
	{
		const Entity entity = entt::entt_traits<Entity>::construct(1, 0);
		VisibilityRegistry registry;
		Check(registry.Register(entity), "Far plane test entity must register");
		eastl::vector<Entity> visible;

		const auto checkCamera = [&](const glm::vec3& position, const glm::vec3& forward)
		{
			CameraComponent camera;
			camera.forward = forward;
			camera.nearPlane = 0.1f;
			camera.farPlane = 336.0f;
			const VisibilityCamera snapshot{camera.GetProjectionMatrix() * camera.GetViewMatrix(position), position,
			                                camera.forward, camera.farPlane};
			const auto checkDepth = [&](float closestDepth, bool expected, const char* message)
			{
				const glm::vec3 center = position + forward * (closestDepth + 0.05f);
				const glm::vec3 halfExtent{0.05f};
				Check(registry.UpdateBounds(entity, WorldAabb{center - halfExtent, center + halfExtent}),
				      "Far plane test entity must update");
				registry.QueryVisible(snapshot, visible);
				Check(expected ? visible.size() == 1 && visible.front() == entity : visible.empty(), message);
			};

			checkDepth(335.99f, true, "AABB intersecting the far plane of a stress-scene camera must be visible");
			checkDepth(336.0f, true, "AABB touching the far plane of a stress-scene camera must be visible");
			checkDepth(336.1f, false, "AABB fully beyond the far plane of a stress-scene camera must be culled");
		};

		checkCamera({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
		checkCamera({100.0f, 5.0f, 20.0f}, {1.0f, 0.0f, 0.0f});
	}

	class SceneRestoreProbe final : public ISystem
	{
	  public:
		void Update(World&, float) override {}

		void OnSceneRestored(World& world) override
		{
			Check(world.GetVisibilityRegistry().Size() == 0, "World must clear visibility before notifying systems");
			called = true;
		}

		bool called = false;
	};

	void TestWorldVisibility()
	{
		World world;
		World otherWorld;
		const Entity entity = world.CreateEntity();
		const Entity otherEntity = otherWorld.CreateEntity();
		const WorldAabb leftBounds{{-0.5f, -0.5f, -5.5f}, {0.5f, 0.5f, -4.5f}};
		const WorldAabb rightBounds{{19.5f, -0.5f, -5.5f}, {20.5f, 0.5f, -4.5f}};
		const VisibilityCamera leftCamera = MakeCamera({0.0f, 0.0f, 0.0f});
		const VisibilityCamera rightCamera = MakeCamera({20.0f, 0.0f, 0.0f});
		Check(world.GetVisibilityRegistry().Register(entity, leftBounds), "World must accept a visibility entry");
		Check(otherWorld.GetVisibilityRegistry().Register(otherEntity, rightBounds),
		      "Another world must accept its own visibility entry");

		const World& scene = world;
		eastl::vector<Entity> leftVisible;
		eastl::vector<Entity> rightVisible;
		scene.GetVisibilityRegistry().QueryVisible(leftCamera, leftVisible);
		scene.GetVisibilityRegistry().QueryVisible(rightCamera, rightVisible);
		CheckVisible(leftVisible, {entity}, "Left camera must see the world entry");
		CheckVisible(rightVisible, {}, "Right camera must cull the same world entry");
		otherWorld.GetVisibilityRegistry().QueryVisible(rightCamera, rightVisible);
		CheckVisible(rightVisible, {otherEntity}, "Right camera must see the other world's entry");

		auto probe = eastl::make_unique<SceneRestoreProbe>();
		const SceneRestoreProbe& observer = *probe;
		world.AddSystem(std::move(probe));
		world.GetEventBus().Publish(SceneRestoredEvent{});
		Check(observer.called, "Scene restore must notify systems");
		scene.GetVisibilityRegistry().QueryVisible(leftCamera, leftVisible);
		CheckVisible(leftVisible, {}, "Restored world must discard the previous scene's entry");
		otherWorld.GetVisibilityRegistry().QueryVisible(rightCamera, rightVisible);
		CheckVisible(rightVisible, {otherEntity}, "Restoring one world must preserve another world's entries");
	}
} // namespace

int main()
{
	const Entity original = entt::entt_traits<Entity>::construct(3, 0);
	const Entity replacement = entt::entt_traits<Entity>::construct(3, 1);
	const WorldAabb bounds{{-2.0f, -1.0f, -5.0f}, {2.0f, 1.0f, -3.0f}};
	const WorldAabb movedBounds{{4.0f, -1.0f, -5.0f}, {8.0f, 1.0f, -3.0f}};
	VisibilityRegistry registry;

	Check(registry.Size() == 0 && registry.FindBounds(original) == nullptr, "New registry must be empty");
	Check(!registry.Register(NullEntity), "NullEntity must be rejected");
	Check(!registry.UpdateBounds(original, bounds), "Updating an absent entity must fail");
	Check(!registry.Remove(original), "Removing an absent entity must fail");

	Check(registry.Register(original), "Registration with unknown bounds must succeed");
	Check(HasUnknownBounds(registry, original), "Unknown bounds must preserve the entry");
	Check(!registry.Register(original, bounds), "Duplicate registration must fail");
	Check(HasUnknownBounds(registry, original) && registry.Size() == 1, "Duplicate registration must preserve the entry");
	Check(registry.UpdateBounds(original, bounds), "Setting initial bounds must succeed");
	Check(HasBounds(registry, original, bounds), "Updated bounds must be stored");
	Check(registry.UpdateBounds(original, movedBounds), "Replacing known bounds must succeed");
	Check(HasBounds(registry, original, movedBounds), "Replacement bounds must be stored");
	Check(registry.UpdateBounds(original, std::nullopt), "Bounds can become unknown again");
	Check(HasUnknownBounds(registry, original), "Clearing bounds must preserve the entry");

	Check(registry.Remove(original), "Removing a registered entity must succeed");
	Check(registry.FindBounds(original) == nullptr, "Removing an entity must erase its entry");
	Check(registry.Register(replacement, movedBounds), "New entity version must register");
	Check(!registry.UpdateBounds(original, bounds), "Stale handle must not update the new version");
	Check(!registry.Remove(original), "Stale handle must not remove the new version");
	Check(HasBounds(registry, replacement, movedBounds), "New entity version must retain its bounds");

	registry.Clear();
	Check(registry.Size() == 0 && registry.FindBounds(replacement) == nullptr, "Clear must erase all entries");
	Check(registry.Register(replacement, bounds), "Registry must accept new entries after Clear");

	TestLocalGeometryBounds();
	TestImportedModelBounds();
	TestCameraQueries();
	TestFrustumBoundaries();
	TestFarPlanePrecision();
	TestWorldVisibility();
}
