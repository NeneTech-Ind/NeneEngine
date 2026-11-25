// VisibilityRegistryTests.cpp

#include "ECS/Components/CameraComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/VisibilityRegistry.h"
#include "ECS/World.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

namespace
{
	using namespace NeneEngine::ECS;

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << message << '\n';
		std::exit(EXIT_FAILURE);
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
		return {camera.GetProjectionMatrix() * camera.GetViewMatrix(worldPosition)};
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

	TestCameraQueries();
	TestFrustumBoundaries();
	TestWorldVisibility();
}
