#include "Scripting/Bindings/ScriptEntity.h"

#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>

namespace NeneEngine
{

	namespace
	{
		ECS::TransformComponent& RequireTransform(ECS::World& world, ECS::Entity entity)
		{
			auto* transform = world.GetComponent<ECS::TransformComponent>(entity);
			if (!transform) throw std::runtime_error("Entity has no TransformComponent");
			return *transform;
		}

		void ValidateVector(const glm::vec3& value)
		{
			if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
				throw std::invalid_argument("Entity transform values must be finite");
		}
	}

	ScriptEntity::ScriptEntity(ECS::World& world, ECS::Entity entity)
	    : m_lifetime(world.GetLifetime()), m_entity(entity)
	{
		if (const auto lifetime = m_lifetime.lock()) m_sceneGeneration = lifetime->sceneGeneration;
	}

	ECS::World* ScriptEntity::GetLiveWorld() const
	{
		const auto lifetime = m_lifetime.lock();
		if (!lifetime || lifetime->sceneGeneration != m_sceneGeneration) return nullptr;
		return lifetime->world;
	}

	bool ScriptEntity::IsValid() const
	{
		const auto* world = GetLiveWorld();
		return world && world->GetRegistry().valid(m_entity);
	}

	ECS::World& ScriptEntity::GetWorld() const
	{
		auto* world = GetLiveWorld();
		if (!world || !world->GetRegistry().valid(m_entity)) throw std::runtime_error("Entity is no longer valid");
		return *world;
	}

	ECS::Entity ScriptEntity::GetId() const
	{
		(void)GetWorld();
		return m_entity;
	}

	std::string ScriptEntity::GetName() const
	{
		const auto* tag = GetWorld().GetComponent<ECS::TagComponent>(m_entity);
		return tag ? tag->name : std::string{};
	}

	glm::vec3 ScriptEntity::GetPosition() const
	{
		return RequireTransform(GetWorld(), m_entity).position;
	}

	void ScriptEntity::SetPosition(const glm::vec3& position) const
	{
		auto& transform = RequireTransform(GetWorld(), m_entity);
		ValidateVector(position);
		transform.position = position;
	}

	glm::vec3 ScriptEntity::GetRotation() const
	{
		return glm::degrees(glm::eulerAngles(RequireTransform(GetWorld(), m_entity).rotation));
	}

	void ScriptEntity::SetRotation(const glm::vec3& degrees) const
	{
		auto& transform = RequireTransform(GetWorld(), m_entity);
		ValidateVector(degrees);
		transform.rotation = glm::quat(glm::radians(degrees));
	}

}
