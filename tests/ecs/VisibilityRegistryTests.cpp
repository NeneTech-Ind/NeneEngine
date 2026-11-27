// VisibilityRegistryTests.cpp

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/VisibilityRegistry.h"
#include "ECS/World.h"
#include "Editor/EditorSceneSnapshot.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Runtime/GeometryBounds.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>

void TestAsyncMeshGeometry();

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

	void TestWorldBoundsFromGeometry()
	{
		using NeneEngine::LocalAabb;
		World world;
		const Entity entity = world.CreateEntity();
		auto& registry = world.GetVisibilityRegistry();
		const LocalAabb original{{0.0f, 0.0f, -1.0f}, {2.0f, 1.0f, 1.0f}};

		TransformComponent parent;
		parent.position.z = -5.0f;
		parent.scale = {-2.0f, 3.0f, 0.5f};
		TransformComponent child;
		child.position.x = 1.0f;
		child.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		const glm::mat4 childLocal = child.GetModelMatrix();
		const auto childWorld = [&]() { return parent.GetModelMatrix() * childLocal; };

		Check(!registry.UpdateGeometryBounds(entity, original, childWorld()),
		      "Updating geometry must require a registered entity");
		Check(registry.Register(entity), "Child entity must register");
		Check(registry.UpdateGeometryBounds(entity, original, childWorld()), "Child bounds must update");
		const auto* bounds = registry.FindBounds(entity);
		const glm::vec3 tolerance(0.0001f);
		Check(bounds && *bounds &&
		          glm::all(glm::lessThan(glm::abs((*bounds)->min - glm::vec3(-2.0f, 0.0f, -5.5f)), tolerance)) &&
		          glm::all(glm::lessThan(glm::abs((*bounds)->max - glm::vec3(0.0f, 6.0f, -4.5f)), tolerance)),
		      "World bounds must include child rotation and parent negative, nonuniform scale");

		const auto camera = MakeCamera({0.0f, 0.0f, 0.0f});
		eastl::vector<Entity> visible;
		parent.position.z = -10.5f;
		Check(registry.UpdateGeometryBounds(entity, original, childWorld()), "Parent motion must update child bounds");
		registry.QueryVisible(camera, visible);
		CheckVisible(visible, {entity}, "A transformed child touching the far plane must be visible");

		parent.position.z = -10.6f;
		const glm::mat4 outsideWorld = childWorld();
		Check(registry.UpdateGeometryBounds(entity, original, outsideWorld), "Child bounds must follow its parent");
		registry.QueryVisible(camera, visible);
		CheckVisible(visible, {}, "A transformed child fully beyond the far plane must be culled");

		Check(registry.UpdateGeometryBounds(entity, std::nullopt, outsideWorld), "Loading geometry must clear old bounds");
		Check(HasUnknownBounds(registry, entity), "Loading geometry must store unknown bounds");
		registry.QueryVisible(camera, visible);
		CheckVisible(visible, {entity}, "Loading geometry must remain a visibility candidate");

		const LocalAabb replacement{{0.0f, 0.0f, 1.0f}, {2.0f, 1.0f, 2.0f}};
		Check(registry.UpdateGeometryBounds(entity, replacement, outsideWorld),
		      "Ready geometry must update bounds without a transform change");
		registry.QueryVisible(camera, visible);
		CheckVisible(visible, {entity}, "Replacement geometry intersecting the far plane must be visible");

		glm::mat4 invalidWorld = outsideWorld;
		invalidWorld[3].x = std::numeric_limits<float>::infinity();
		Check(registry.UpdateGeometryBounds(entity, replacement, invalidWorld) && HasUnknownBounds(registry, entity),
		      "Non-finite world positions must produce unknown bounds");
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

	void TestVisibilityComponentChanges()
	{
		World world;
		auto& registry = world.GetRegistry();
		const auto& visibility = world.GetVisibilityRegistry();
		const auto camera = MakeCamera({0.0f, 0.0f, 0.0f});
		eastl::vector<Entity> visible;

		const Entity first = world.CreateEntity();
		world.AddComponent<MeshRendererComponent>(first);
		world.SyncVisibility();
		Check(visibility.FindBounds(first) == nullptr, "A mesh without a transform must be skipped");
		world.AddComponent<TransformComponent>(first).position.z = -5.0f;
		const Entity second = world.CreateEntity();
		world.AddComponent<TransformComponent>(second).position = {20.0f, 0.0f, -5.0f};
		world.AddComponent<MeshRendererComponent>(second);
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {first}, "Visibility must use final component values in both construction orders");

		registry.patch<TransformComponent>(first, [](auto& transform) { transform.position.x = 20.0f; });
		registry.patch<TransformComponent>(second, [](auto& transform) { transform.position.x = 0.0f; });
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {second}, "Patched transforms must change the visible set");

		registry.patch<MeshRendererComponent>(first, [](auto& mesh) { mesh.meshPath = "pending.gltf"; });
		registry.patch<MeshRendererComponent>(second, [](auto& mesh) { mesh.visible = false; });
		world.SyncVisibility();
		Check(HasUnknownBounds(visibility, first), "An imported mesh must clear the previous primitive bounds");
		Check(visibility.FindBounds(second) == nullptr, "Hiding an entity must remove its entry");
		registry.patch<MeshRendererComponent>(second, [](auto& mesh) { mesh.visible = true; });
		world.SyncVisibility();
		Check(visibility.FindBounds(second) != nullptr, "Showing an entity must restore its entry");
		world.RemoveComponent<MeshRendererComponent>(second);
		world.SyncVisibility();
		Check(visibility.FindBounds(second) == nullptr, "Removing the mesh must remove its entry");

		world.DeferDestroyEntity(first);
		world.Update(0.0f);
		const Entity replacement = world.CreateEntity();
		Check(entt::to_entity(first) == entt::to_entity(replacement) && first != replacement,
		      "The replacement must reuse the destroyed entity's slot with a new version");
		world.AddComponent<TransformComponent>(replacement).position.z = -5.0f;
		world.AddComponent<MeshRendererComponent>(replacement);
		world.SyncVisibility();
		Check(visibility.FindBounds(first) == nullptr, "Destruction must remove the old entity version");
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {replacement}, "The old entity version must not interfere with the replacement");
		world.RemoveComponent<TransformComponent>(replacement);
		world.SyncVisibility();
		Check(visibility.Size() == 0, "Removing the transform must remove its entry");
	}

	void TestVisibilityAfterRegistrySwap()
	{
		World world;
		const auto& visibility = world.GetVisibilityRegistry();
		for (int load = 0; load < 2; ++load)
		{
			const Entity previous = world.CreateEntity();
			world.AddComponent<TransformComponent>(previous).position.z = -5.0f;
			world.AddComponent<MeshRendererComponent>(previous);
			world.SyncVisibility();

			Entity loaded;
			{
				World prepared;
				prepared.CreateEntity();
				loaded = prepared.CreateEntity();
				prepared.AddComponent<TransformComponent>(loaded).position = {20.0f, 0.0f, -5.0f};
				prepared.AddComponent<MeshRendererComponent>(loaded);
				world.SwapSceneRegistry(prepared);
			}
			Check(visibility.Size() == 0, "A scene swap must clear the old visibility state immediately");
			world.SyncVisibility();
			Check(visibility.Size() == 1 && visibility.FindBounds(loaded) != nullptr,
			      "The first synchronization after a scene swap must rebuild the new scene");
			world.GetRegistry().patch<TransformComponent>(loaded, [](auto& transform) { transform.position.x = 0.0f; });
			world.SyncVisibility();
			eastl::vector<Entity> visible;
			visibility.QueryVisible(MakeCamera({0.0f, 0.0f, 0.0f}), visible);
			CheckVisible(visible, {loaded}, "Observers must track component updates after each scene swap");
		}
	}

	void TestVisibilityHierarchyChanges()
	{
		World world;
		auto& registry = world.GetRegistry();
		const auto& visibility = world.GetVisibilityRegistry();
		const auto camera = MakeCamera({0.0f, 0.0f, 0.0f});
		eastl::vector<Entity> visible;

		const Entity root = world.CreateEntity();
		const Entity middle = world.CreateEntity();
		const Entity leaf = world.CreateEntity();
		const Entity neighbor = world.CreateEntity();
		const Entity otherParent = world.CreateEntity();
		world.AddComponent<TransformComponent>(root).position.z = -5.0f;
		world.AddComponent<TransformComponent>(middle).position.x = 1.0f;
		world.AddComponent<TransformComponent>(leaf).position.x = 1.0f;
		world.AddComponent<TransformComponent>(neighbor).position.z = -5.0f;
		world.AddComponent<TransformComponent>(otherParent).position = {-20.0f, 0.0f, -5.0f};
		world.AddComponent<HierarchyComponent>(middle, HierarchyComponent{root, {leaf}});
		world.AddComponent<HierarchyComponent>(leaf, HierarchyComponent{middle});
		world.AddComponent<MeshRendererComponent>(leaf);
		world.AddComponent<MeshRendererComponent>(neighbor);
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {leaf, neighbor}, "A three-level hierarchy must use world-space bounds");
		const WorldAabb neighborBounds{{-0.5f, -0.5f, -5.0f}, {0.5f, 0.5f, -5.0f}};

		registry.patch<TransformComponent>(root, [](auto& transform) {
			transform.position.x = 20.0f;
			transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0.0f, 0.0f, 1.0f});
			transform.scale = {2.0f, 3.0f, 1.0f};
		});
		world.SyncVisibility();
		const auto* leafBounds = visibility.FindBounds(leaf);
		const auto near = [](const glm::vec3& actual, const glm::vec3& expected) {
			return glm::all(glm::lessThan(glm::abs(actual - expected), glm::vec3(0.0001f)));
		};
		Check(leafBounds && *leafBounds && near((*leafBounds)->min, {18.5f, 3.0f, -5.0f}) &&
		          near((*leafBounds)->max, {21.5f, 5.0f, -5.0f}),
		      "Moving, rotating and scaling a parent must update the descendant's AABB");
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {neighbor}, "Moving a non-renderable parent must cull its descendants");
		Check(HasBounds(visibility, neighbor, neighborBounds), "An independent neighbor must keep its bounds");

		registry.patch<HierarchyComponent>(middle, [otherParent](auto& hierarchy) { hierarchy.parent = otherParent; });
		world.SyncVisibility();
		Check(HasBounds(visibility, leaf, {{-18.5f, -0.5f, -5.0f}, {-17.5f, 0.5f, -5.0f}}),
		      "Changing the parent must invalidate descendant bounds");
		world.RemoveComponent<TransformComponent>(otherParent);
		world.SyncVisibility();
		Check(HasBounds(visibility, leaf, {{1.5f, -0.5f, 0.0f}, {2.5f, 0.5f, 0.0f}}),
		      "A parent with no transform must terminate the transform chain");
		world.AddComponent<TransformComponent>(otherParent).position.z = -5.0f;
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {leaf, neighbor}, "Restoring a parent's transform must restore descendant visibility");

		world.AddComponent<HierarchyComponent>(otherParent, HierarchyComponent{leaf});
		world.SyncVisibility();
		Check(HasUnknownBounds(visibility, leaf), "A hierarchy cycle must have unknown bounds");
		world.RemoveComponent<HierarchyComponent>(otherParent);
		world.SyncVisibility();
		Check(HasBounds(visibility, leaf, {{1.5f, -0.5f, -5.0f}, {2.5f, 0.5f, -5.0f}}),
		      "Removing the cyclic parent link must restore the descendant's bounds");
		world.GetEventBus().Publish(SceneRestoredEvent{});
		world.SyncVisibility();
		registry.patch<TransformComponent>(otherParent, [](auto& transform) { transform.position.x = 20.0f; });
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {neighbor}, "Scene restore must rebuild the hierarchy for later parent movement");

		world.DestroyEntity(middle);
		world.SyncVisibility();
		Check(visibility.FindBounds(leaf) == nullptr, "Destroying a subtree must remove descendant entries");
	}

	void TestVisibilityAfterPlayStop()
	{
		World world;
		const auto& visibility = world.GetVisibilityRegistry();
		const Entity moved = world.CreateEntity();
		world.AddComponent<TransformComponent>(moved).position.z = -5.0f;
		world.AddComponent<MeshRendererComponent>(moved);
		const Entity removed = world.CreateEntity();
		world.AddComponent<TransformComponent>(removed).position.z = -5.0f;
		world.AddComponent<MeshRendererComponent>(removed);
		world.SyncVisibility();

		NeneEngine::EditorSceneSnapshot snapshot;
		snapshot.Capture(world, {});
		world.GetRegistry().patch<TransformComponent>(moved, [](auto& transform) { transform.position.x = 20.0f; });
		world.DestroyEntity(removed);
		const Entity spawned = world.CreateEntity();
		world.AddComponent<TransformComponent>(spawned).position.z = -5.0f;
		world.AddComponent<MeshRendererComponent>(spawned);
		world.SyncVisibility();

		const auto camera = MakeCamera({0.0f, 0.0f, 0.0f});
		eastl::vector<Entity> visible;
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {spawned}, "The running scene must reflect changes made in play");
		snapshot.Restore(world);
		world.SyncVisibility();
		visibility.QueryVisible(camera, visible);
		CheckVisible(visible, {moved, removed}, "Stop must restore the original scene and its visible set");
		Check(visibility.FindBounds(spawned) == nullptr, "Stop must discard entries created in play");
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
	TestAsyncMeshGeometry();
	TestCameraQueries();
	TestFrustumBoundaries();
	TestWorldBoundsFromGeometry();
	TestFarPlanePrecision();
	TestWorldVisibility();
	TestVisibilityComponentChanges();
	TestVisibilityAfterRegistrySwap();
	TestVisibilityHierarchyChanges();
	TestVisibilityAfterPlayStop();
}
