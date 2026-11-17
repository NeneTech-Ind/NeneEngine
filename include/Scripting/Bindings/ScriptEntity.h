#pragma once

#include "ECS/Entity.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>
#include <string>

namespace NeneEngine
{

	namespace ECS
	{
		class World;
		struct WorldLifetime;
	}

	class ScriptEntity final
	{
	  public:
		ScriptEntity() = default;
		ScriptEntity(ECS::World& world, ECS::Entity entity);

		[[nodiscard]] bool IsValid() const;
		[[nodiscard]] ECS::World& GetWorld() const;
		[[nodiscard]] ECS::Entity GetId() const;
		[[nodiscard]] std::string GetName() const;
		[[nodiscard]] glm::vec3 GetPosition() const;
		void SetPosition(const glm::vec3& position) const;
		[[nodiscard]] glm::vec3 GetRotation() const;
		void SetRotation(const glm::vec3& degrees) const;

	  private:
		[[nodiscard]] ECS::World* GetLiveWorld() const;

		std::weak_ptr<const ECS::WorldLifetime> m_lifetime;
		ECS::Entity m_entity = ECS::NullEntity;
		uint64_t m_sceneGeneration = 0;
	};

}
