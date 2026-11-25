// VisibilityRegistryTests.cpp

#include "ECS/VisibilityRegistry.h"

#include <cstdlib>
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
}
