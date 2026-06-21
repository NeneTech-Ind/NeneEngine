// CollisionSystem.cpp

#include "ECS/Systems/CollisionSystem.h"

#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/CollisionEvent.h"
#include "ECS/World.h"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <iterator>

namespace NeneEngine::ECS
{
	namespace
	{
		struct CollisionBounds
		{
			glm::vec3 center = {0.0f, 0.0f, 0.0f};
			glm::vec3 halfExtents = {0.0f, 0.0f, 0.0f};
			float radius = 0.0f;
		};

		[[nodiscard]] glm::vec3 ComputeColliderCenter(const TransformComponent& transform,
		                                              const ColliderComponent& collider)
		{
			return transform.position + collider.offset;
		}

		[[nodiscard]] CollisionBounds ComputeCollisionBounds(const TransformComponent& transform,
		                                                     const ColliderComponent& collider)
		{
			const glm::vec3 absoluteScale = glm::abs(transform.scale);
			const float maxScaleAxis = (std::max)(absoluteScale.x, (std::max)(absoluteScale.y, absoluteScale.z));

			CollisionBounds bounds{};
			bounds.center = ComputeColliderCenter(transform, collider);
			bounds.halfExtents = collider.halfExtents * absoluteScale;
			bounds.radius = collider.radius * maxScaleAxis;
			return bounds;
		}

		[[nodiscard]] bool CheckSphereSphereCollision(const CollisionBounds& lhs, const CollisionBounds& rhs)
		{
			const float combinedRadius = lhs.radius + rhs.radius;
			const glm::vec3 delta = lhs.center - rhs.center;
			return glm::dot(delta, delta) <= combinedRadius * combinedRadius;
		}

		[[nodiscard]] bool CheckBoxBoxCollision(const CollisionBounds& lhs, const CollisionBounds& rhs)
		{
			const glm::vec3 delta = glm::abs(lhs.center - rhs.center);
			const glm::vec3 allowedDelta = lhs.halfExtents + rhs.halfExtents;
			return delta.x <= allowedDelta.x && delta.y <= allowedDelta.y && delta.z <= allowedDelta.z;
		}

		[[nodiscard]] bool CheckBoxSphereCollision(const CollisionBounds& box, const CollisionBounds& sphere)
		{
			const glm::vec3 minBounds = box.center - box.halfExtents;
			const glm::vec3 maxBounds = box.center + box.halfExtents;
			const glm::vec3 closestPoint = glm::clamp(sphere.center, minBounds, maxBounds);
			const glm::vec3 delta = closestPoint - sphere.center;
			return glm::dot(delta, delta) <= sphere.radius * sphere.radius;
		}

		[[nodiscard]] bool CheckCollision(const TransformComponent& lhsTransform, const ColliderComponent& lhsCollider,
		                                  const TransformComponent& rhsTransform, const ColliderComponent& rhsCollider)
		{
			const CollisionBounds lhsBounds = ComputeCollisionBounds(lhsTransform, lhsCollider);
			const CollisionBounds rhsBounds = ComputeCollisionBounds(rhsTransform, rhsCollider);

			if (lhsCollider.type == ColliderType::Sphere && rhsCollider.type == ColliderType::Sphere)
				return CheckSphereSphereCollision(lhsBounds, rhsBounds);

			if (lhsCollider.type == ColliderType::Box && rhsCollider.type == ColliderType::Box)
				return CheckBoxBoxCollision(lhsBounds, rhsBounds);

			if (lhsCollider.type == ColliderType::Box && rhsCollider.type == ColliderType::Sphere)
				return CheckBoxSphereCollision(lhsBounds, rhsBounds);

			return CheckBoxSphereCollision(rhsBounds, lhsBounds);
		}
	} // namespace

	void CollisionSystem::Update(World& world, float /*deltaTime*/)
	{
		auto view = world.GetRegistry().view<const TransformComponent, const ColliderComponent>();
		const auto entities = view.each();

		for (auto first = entities.begin(); first != entities.end(); ++first)
		{
			const auto [firstEntity, firstTransform, firstCollider] = *first;

			for (auto second = std::next(first); second != entities.end(); ++second)
			{
				const auto [secondEntity, secondTransform, secondCollider] = *second;

				if (!CheckCollision(firstTransform, firstCollider, secondTransform, secondCollider)) continue;

				world.GetEventBus().Publish(CollisionEvent{firstEntity, secondEntity});
			}
		}
	}

} // namespace NeneEngine::ECS
