// PhysicsSystem.cpp

#include "ECS/Systems/PhysicsSystem.h"

#include "Core/NeneLogger.h"
#include "Core/Profiler.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/ContactStateComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/CollisionEvent.h"
#include "ECS/World.h"

// Jolt.h must be included before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <type_traits>
#include <EASTL/unordered_map.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr glm::vec3 kGravity{0.0f, -9.81f, 0.0f};
		constexpr float kMinShapeExtent = 0.01f;
		// A contact normal steeper than ~60 degrees from vertical does not count as ground.
		constexpr float kGroundNormalThreshold = 0.5f;

		constexpr JPH::uint kMaxBodies = 4096;
		constexpr JPH::uint kMaxBodyPairs = 4096;
		constexpr JPH::uint kMaxContactConstraints = 4096;
		constexpr size_t kTempAllocatorBytes = 10 * 1024 * 1024;

		namespace ObjectLayers
		{
			constexpr JPH::ObjectLayer NonMoving = 0;
			constexpr JPH::ObjectLayer Moving = 1;
		} // namespace ObjectLayers

		namespace BroadPhaseLayers
		{
			constexpr JPH::BroadPhaseLayer NonMoving(0);
			constexpr JPH::BroadPhaseLayer Moving(1);
			constexpr JPH::uint Count = 2;
		} // namespace BroadPhaseLayers

		class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
		{
		  public:
			JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::Count; }

			JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
			{
				return layer == ObjectLayers::NonMoving ? BroadPhaseLayers::NonMoving : BroadPhaseLayers::Moving;
			}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
			{
				return layer == BroadPhaseLayers::NonMoving ? "NonMoving" : "Moving";
			}
#endif
		};

		class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		  public:
			bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
			{
				// Static bodies never need to be tested against other static bodies.
				return layer != ObjectLayers::NonMoving || broadPhaseLayer == BroadPhaseLayers::Moving;
			}
		};

		class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
		{
		  public:
			bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
			{
				return first == ObjectLayers::Moving || second == ObjectLayers::Moving;
			}
		};

		JPH::Vec3 ToJolt(const glm::vec3& value)
		{
			return JPH::Vec3(value.x, value.y, value.z);
		}

		JPH::Quat ToJolt(const glm::quat& value)
		{
			return JPH::Quat(value.x, value.y, value.z, value.w);
		}

		glm::vec3 ToGlm(JPH::Vec3Arg value)
		{
			return {value.GetX(), value.GetY(), value.GetZ()};
		}

#ifdef JPH_DOUBLE_PRECISION
		glm::vec3 ToGlm(JPH::RVec3Arg value)
		{
			return {static_cast<float>(value.GetX()), static_cast<float>(value.GetY()),
			        static_cast<float>(value.GetZ())};
		}
#endif

		glm::quat ToGlm(JPH::QuatArg value)
		{
			return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
		}

		glm::quat NormalizedOrIdentity(const glm::quat& rotation)
		{
			const float lengthSquared = glm::dot(rotation, rotation);
			if (lengthSquared < 1e-8f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			return rotation / std::sqrt(lengthSquared);
		}

		bool NearlyEqual(const glm::vec3& lhs, const glm::vec3& rhs)
		{
			constexpr float kEpsilon = 1e-5f;
			const glm::vec3 delta = glm::abs(lhs - rhs);
			return delta.x <= kEpsilon && delta.y <= kEpsilon && delta.z <= kEpsilon;
		}

		bool NearlyEqual(const glm::quat& lhs, const glm::quat& rhs)
		{
			// q and -q describe the same rotation.
			return std::abs(glm::dot(lhs, rhs)) >= 1.0f - 1e-6f;
		}

		JPH::uint64 ToUserData(Entity entity)
		{
			return static_cast<JPH::uint64>(entt::to_integral(entity));
		}

		Entity ToEntity(JPH::uint64 userData)
		{
			return static_cast<Entity>(static_cast<std::underlying_type_t<Entity>>(userData));
		}

		JPH::uint64 MakePairKey(const JPH::BodyID& first, const JPH::BodyID& second)
		{
			const JPH::uint64 lhs = first.GetIndexAndSequenceNumber();
			const JPH::uint64 rhs = second.GetIndexAndSequenceNumber();
			return lhs < rhs ? (lhs << 32) | rhs : (rhs << 32) | lhs;
		}

		bool HasParent(const entt::registry& registry, Entity entity)
		{
			const auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
			return hierarchy != nullptr && hierarchy->parent != NullEntity;
		}

		// Everything that requires rebuilding the Jolt body when it changes.
		struct BodyDefinition
		{
			ColliderType colliderType = ColliderType::Box;
			glm::vec3 halfExtents = {0.0f, 0.0f, 0.0f};
			float radius = 0.0f;
			glm::vec3 offset = {0.0f, 0.0f, 0.0f};
			glm::vec3 scale = {1.0f, 1.0f, 1.0f};
			bool isDynamic = false;
			float mass = 0.0f;
			bool useGravity = false;
			bool freezeRotation = false;

			bool operator==(const BodyDefinition&) const = default;
		};

		BodyDefinition MakeBodyDefinition(const ColliderComponent& collider, const glm::vec3& scale,
		                                  const RigidbodyComponent* rigidbody)
		{
			BodyDefinition definition{};
			definition.colliderType = collider.type;
			definition.halfExtents = collider.halfExtents;
			definition.radius = collider.radius;
			definition.offset = collider.offset;
			definition.scale = scale;
			if (rigidbody != nullptr)
			{
				definition.isDynamic = true;
				definition.mass = rigidbody->mass;
				definition.useGravity = rigidbody->useGravity;
				definition.freezeRotation = rigidbody->freezeRotation;
			}
			return definition;
		}

		// Jolt bodies have no scale, so the entity scale is baked into the collision shape.
		JPH::RefConst<JPH::Shape> CreateShape(const BodyDefinition& definition)
		{
			const glm::vec3 absoluteScale = glm::abs(definition.scale);

			JPH::RefConst<JPH::Shape> shape;
			if (definition.colliderType == ColliderType::Sphere)
			{
				const float maxScaleAxis = (std::max)({absoluteScale.x, absoluteScale.y, absoluteScale.z});
				shape = new JPH::SphereShape((std::max)(definition.radius * maxScaleAxis, kMinShapeExtent));
			}
			else
			{
				const glm::vec3 halfExtents =
				    glm::max(definition.halfExtents * absoluteScale, glm::vec3(kMinShapeExtent));
				shape = new JPH::BoxShape(ToJolt(halfExtents));
			}

			const glm::vec3 scaledOffset = definition.offset * definition.scale;
			if (scaledOffset != glm::vec3(0.0f))
				shape = new JPH::RotatedTranslatedShape(ToJolt(scaledOffset), JPH::Quat::sIdentity(), shape);

			return shape;
		}

		struct ContactRecord
		{
			JPH::BodyID body1;
			JPH::BodyID body2;
			Entity entity1 = NullEntity;
			Entity entity2 = NullEntity;
			glm::vec3 point = {0.0f, 0.0f, 0.0f};
			// Jolt convention: direction along which body 2 must move to resolve the penetration.
			glm::vec3 normal = {0.0f, 0.0f, 0.0f};
		};

		enum class ContactChangeType : uint8_t
		{
			Added,
			Persisted,
			Removed
		};

		struct ContactChange
		{
			ContactChangeType type = ContactChangeType::Added;
			JPH::uint64 pairKey = 0;
			ContactRecord record{};
		};

		// Jolt calls these from worker threads while bodies are locked, so changes are only queued here
		// and applied on the main thread after the step.
		class ContactListenerImpl final : public JPH::ContactListener
		{
		  public:
			void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold,
			                    JPH::ContactSettings& /*settings*/) override
			{
				Push(ContactChangeType::Added, body1, body2, manifold);
			}

			void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
			                        const JPH::ContactManifold& manifold, JPH::ContactSettings& /*settings*/) override
			{
				Push(ContactChangeType::Persisted, body1, body2, manifold);
			}

			void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
			{
				ContactChange change{};
				change.type = ContactChangeType::Removed;
				change.pairKey = MakePairKey(pair.GetBody1ID(), pair.GetBody2ID());
				change.record.body1 = pair.GetBody1ID();
				change.record.body2 = pair.GetBody2ID();

				std::lock_guard lock(m_mutex);
				m_changes.push_back(change);
			}

			eastl::vector<ContactChange> TakeChanges()
			{
				std::lock_guard lock(m_mutex);
				return eastl::exchange(m_changes, {});
			}

		  private:
			void Push(ContactChangeType type, const JPH::Body& body1, const JPH::Body& body2,
			          const JPH::ContactManifold& manifold)
			{
				ContactChange change{};
				change.type = type;
				change.pairKey = MakePairKey(body1.GetID(), body2.GetID());
				change.record.body1 = body1.GetID();
				change.record.body2 = body2.GetID();
				change.record.entity1 = ToEntity(body1.GetUserData());
				change.record.entity2 = ToEntity(body2.GetUserData());
				change.record.point = ToGlm(manifold.GetWorldSpaceContactPointOn1(0));
				change.record.normal = ToGlm(manifold.mWorldSpaceNormal);

				std::lock_guard lock(m_mutex);
				m_changes.push_back(change);
			}

			std::mutex m_mutex;
			eastl::vector<ContactChange> m_changes;
		};

		int joltUserCount = 0;

		void AcquireJolt()
		{
			if (joltUserCount++ > 0) return;

			JPH::RegisterDefaultAllocator();
			JPH::Factory::sInstance = new JPH::Factory();
			JPH::RegisterTypes();
		}

		void ReleaseJolt()
		{
			if (--joltUserCount > 0) return;

			JPH::UnregisterTypes();
			delete JPH::Factory::sInstance;
			JPH::Factory::sInstance = nullptr;
		}
	} // namespace

	struct PhysicsSystem::Impl
	{
		struct BodyRecord
		{
			JPH::BodyID bodyId;
			BodyDefinition definition{};
			// Last values exchanged with Jolt; a mismatch means gameplay code changed the component.
			glm::vec3 syncedPosition = {0.0f, 0.0f, 0.0f};
			glm::quat syncedRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			glm::vec3 syncedVelocity = {0.0f, 0.0f, 0.0f};
		};

		BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
		ObjectLayerPairFilterImpl objectLayerPairFilter;
		ContactListenerImpl contactListener;
		JPH::TempAllocatorImpl tempAllocator{kTempAllocatorBytes};
		JPH::JobSystemThreadPool jobSystem{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
		                                   static_cast<int>((std::max)(1u, std::thread::hardware_concurrency()) - 1)};
		JPH::PhysicsSystem physicsSystem;

		eastl::unordered_map<Entity, BodyRecord> bodies;
		eastl::unordered_map<JPH::uint64, ContactRecord> contacts;
		eastl::vector<CollisionEvent> pendingEvents;
		float accumulator = 0.0f;

		Impl()
		{
			physicsSystem.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints, broadPhaseLayerInterface,
			                   objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
			physicsSystem.SetGravity(ToJolt(kGravity));
			physicsSystem.SetContactListener(&contactListener);
		}

		~Impl()
		{
			for (const auto& [entity, record] : bodies) DestroyJoltBody(record.bodyId);
		}

		JPH::BodyInterface& Bodies() { return physicsSystem.GetBodyInterface(); }

		void DestroyJoltBody(const JPH::BodyID& bodyId)
		{
			Bodies().RemoveBody(bodyId);
			Bodies().DestroyBody(bodyId);
			eastl::erase_if(contacts, [&bodyId](const auto& entry)
			              { return entry.second.body1 == bodyId || entry.second.body2 == bodyId; });
		}

		void CreateBody(Entity entity, const TransformComponent& transform, const RigidbodyComponent* rigidbody,
		                const BodyDefinition& definition)
		{
			const glm::quat rotation = NormalizedOrIdentity(transform.rotation);
			const JPH::EMotionType motionType =
			    definition.isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
			const JPH::ObjectLayer layer = definition.isDynamic ? ObjectLayers::Moving : ObjectLayers::NonMoving;

			JPH::BodyCreationSettings settings(CreateShape(definition), ToJolt(transform.position), ToJolt(rotation),
			                                   motionType, layer);
			settings.mUserData = ToUserData(entity);

			if (rigidbody != nullptr)
			{
				settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
				settings.mMassPropertiesOverride.mMass = (std::max)(rigidbody->mass, 0.001f);
				settings.mGravityFactor = rigidbody->useGravity ? 1.0f : 0.0f;
				settings.mLinearVelocity = ToJolt(rigidbody->velocity);
				if (rigidbody->freezeRotation)
					settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
					                        JPH::EAllowedDOFs::TranslationZ;
			}

			const JPH::BodyID bodyId = Bodies().CreateAndAddBody(
			    settings, definition.isDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
			if (bodyId.IsInvalid())
			{
				NENE_LOG_ERROR("PhysicsSystem: failed to create body for entity {}, body limit reached",
				               entt::to_integral(entity));
				return;
			}

			BodyRecord record{};
			record.bodyId = bodyId;
			record.definition = definition;
			record.syncedPosition = transform.position;
			record.syncedRotation = transform.rotation;
			record.syncedVelocity = rigidbody != nullptr ? rigidbody->velocity : glm::vec3(0.0f);
			bodies.emplace(entity, record);
		}

		// Mirrors ECS changes into Jolt: creates/destroys bodies and applies teleports or velocity edits.
		void SyncBodiesFromWorld(World& world)
		{
			auto& registry = world.GetRegistry();

			for (auto it = bodies.begin(); it != bodies.end();)
			{
				const Entity entity = it->first;
				const bool stillSimulated = registry.valid(entity) &&
				                            registry.all_of<TransformComponent, ColliderComponent>(entity) &&
				                            !HasParent(registry, entity);
				if (stillSimulated)
				{
					++it;
					continue;
				}

				DestroyJoltBody(it->second.bodyId);
				it = bodies.erase(it);
			}

			auto view = registry.view<TransformComponent, const ColliderComponent>();
			for (auto [entity, transform, collider] : view.each())
			{
				if (HasParent(registry, entity)) continue;

				auto* rigidbody = registry.try_get<RigidbodyComponent>(entity);
				const BodyDefinition definition = MakeBodyDefinition(collider, transform.scale, rigidbody);

				auto recordIt = bodies.find(entity);
				if (recordIt != bodies.end() && recordIt->second.definition != definition)
				{
					DestroyJoltBody(recordIt->second.bodyId);
					bodies.erase(recordIt);
					recordIt = bodies.end();
				}

				if (recordIt == bodies.end())
				{
					CreateBody(entity, transform, rigidbody, definition);
					continue;
				}

				BodyRecord& record = recordIt->second;
				if (!NearlyEqual(transform.position, record.syncedPosition) ||
				    !NearlyEqual(transform.rotation, record.syncedRotation))
				{
					Bodies().SetPositionAndRotation(record.bodyId, ToJolt(transform.position),
					                                ToJolt(NormalizedOrIdentity(transform.rotation)),
					                                JPH::EActivation::Activate);
					record.syncedPosition = transform.position;
					record.syncedRotation = transform.rotation;
				}

				if (rigidbody != nullptr && !NearlyEqual(rigidbody->velocity, record.syncedVelocity))
				{
					Bodies().SetLinearVelocity(record.bodyId, ToJolt(rigidbody->velocity));
					record.syncedVelocity = rigidbody->velocity;
				}
			}
		}

		void ApplyAccelerations(World& world)
		{
			for (const auto& [entity, record] : bodies)
			{
				if (!record.definition.isDynamic) continue;

				const auto* rigidbody = world.GetComponent<RigidbodyComponent>(entity);
				if (rigidbody == nullptr || rigidbody->acceleration == glm::vec3(0.0f)) continue;

				Bodies().AddForce(record.bodyId, ToJolt(rigidbody->acceleration * rigidbody->mass));
			}
		}

		bool IsSleepingPair(const JPH::BodyID& first, const JPH::BodyID& second)
		{
			JPH::BodyInterface& bodyInterface = Bodies();
			return bodyInterface.IsAdded(first) && bodyInterface.IsAdded(second) && !bodyInterface.IsActive(first) &&
			       !bodyInterface.IsActive(second);
		}

		void ProcessContactChanges()
		{
			for (const ContactChange& change : contactListener.TakeChanges())
			{
				switch (change.type)
				{
				case ContactChangeType::Added:
				{
					// Jolt re-adds contacts when sleeping bodies wake up; only report pairs that were not touching.
					const bool isNewContact = contacts.insert_or_assign(change.pairKey, change.record).second;
					if (isNewContact)
						pendingEvents.push_back(CollisionEvent{change.record.entity1, change.record.entity2,
						                                       change.record.point, change.record.normal});
					break;
				}
				case ContactChangeType::Persisted:
					contacts.insert_or_assign(change.pairKey, change.record);
					break;
				case ContactChangeType::Removed:
					// Jolt also drops contacts of bodies that fall asleep; keep those so resting bodies stay grounded.
					if (!IsSleepingPair(change.record.body1, change.record.body2)) contacts.erase(change.pairKey);
					break;
				}
			}
		}

		void ReadBackDynamicBodies(World& world)
		{
			for (auto& [entity, record] : bodies)
			{
				if (!record.definition.isDynamic) continue;

				auto* transform = world.GetComponent<TransformComponent>(entity);
				auto* rigidbody = world.GetComponent<RigidbodyComponent>(entity);
				if (transform == nullptr || rigidbody == nullptr) continue;

				JPH::RVec3 position;
				JPH::Quat rotation;
				Bodies().GetPositionAndRotation(record.bodyId, position, rotation);

				transform->position = ToGlm(position);
				transform->rotation = ToGlm(rotation);
				rigidbody->velocity = ToGlm(Bodies().GetLinearVelocity(record.bodyId));

				record.syncedPosition = transform->position;
				record.syncedRotation = transform->rotation;
				record.syncedVelocity = rigidbody->velocity;
			}
		}

		void UpdateContactStates(World& world)
		{
			auto& registry = world.GetRegistry();
			for (const auto& [entity, record] : bodies) registry.emplace_or_replace<ContactStateComponent>(entity);

			for (const auto& [pairKey, contact] : contacts)
			{
				if (auto* state = registry.try_get<ContactStateComponent>(contact.entity1); state != nullptr)
				{
					++state->contactCount;
					// The normal points from body 1 to body 2, so body 1 stands on body 2 when it points down.
					state->isGrounded |= -contact.normal.y > kGroundNormalThreshold;
				}

				if (auto* state = registry.try_get<ContactStateComponent>(contact.entity2); state != nullptr)
				{
					++state->contactCount;
					state->isGrounded |= contact.normal.y > kGroundNormalThreshold;
				}
			}
		}

		void PublishPendingEvents(World& world)
		{
			const eastl::vector<CollisionEvent> events = eastl::exchange(pendingEvents, {});
			for (const CollisionEvent& event : events)
			{
				if (!world.GetRegistry().valid(event.entityA) || !world.GetRegistry().valid(event.entityB)) continue;
				world.GetEventBus().Publish(event);
			}
		}
	};

	PhysicsSystem::PhysicsSystem()
	{
		AcquireJolt();
		m_impl = eastl::make_unique<Impl>();
	}

	PhysicsSystem::~PhysicsSystem()
	{
		m_impl.reset();
		ReleaseJolt();
	}

	void PhysicsSystem::Update(World& world, float deltaTime)
	{
		NENE_PROFILE_SCOPE("Physics");
		Impl& impl = *m_impl;

		impl.SyncBodiesFromWorld(world);

		// Fixed step keeps the simulation stable regardless of frame rate; the cap avoids a spiral of death
		// after long stalls such as window dragging.
		impl.accumulator = (std::min)(impl.accumulator + (std::max)(deltaTime, 0.0f), FixedTimeStep * MaxStepsPerFrame);

		bool stepped = false;
		while (impl.accumulator >= FixedTimeStep)
		{
			impl.ApplyAccelerations(world);

			const JPH::EPhysicsUpdateError error =
			    impl.physicsSystem.Update(FixedTimeStep, 1, &impl.tempAllocator, &impl.jobSystem);
			if (error != JPH::EPhysicsUpdateError::None)
				NENE_LOG_WARN("PhysicsSystem: Jolt update reported error flags {}", static_cast<uint32_t>(error));

			impl.ProcessContactChanges();
			impl.accumulator -= FixedTimeStep;
			stepped = true;
		}

		if (!stepped) return;

		impl.ReadBackDynamicBodies(world);
		impl.UpdateContactStates(world);
		impl.PublishPendingEvents(world);
	}

} // namespace NeneEngine::ECS
