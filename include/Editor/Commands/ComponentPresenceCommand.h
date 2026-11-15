// ComponentPresenceCommand.h

#pragma once

#include "ECS/Entity.h"
#include "ECS/World.h"
#include "Editor/Commands/IEditorCommand.h"

#include <string>
#include <utility>

namespace NeneEngine
{
	template <typename Component> class ComponentPresenceCommand final : public IEditorCommand
	{
	  public:
		enum class Action
		{
			Add,
			Remove
		};

		ComponentPresenceCommand(std::string name, ECS::Entity entity, Action action, Component value)
		    : m_name(std::move(name)), m_entity(entity), m_action(action), m_value(std::move(value))
		{
		}

		[[nodiscard]] const char* GetName() const override { return m_name.c_str(); }

		void Execute(ECS::World& world) override { SetPresent(world, m_action == Action::Add); }
		void Undo(ECS::World& world) override { SetPresent(world, m_action == Action::Remove); }

	  private:
		void SetPresent(ECS::World& world, bool present)
		{
			entt::registry& registry = world.GetRegistry();
			if (!registry.valid(m_entity)) return;

			if (present)
			{
				registry.emplace_or_replace<Component>(m_entity, m_value);
				return;
			}

			if (const Component* component = registry.try_get<Component>(m_entity)) m_value = *component;
			registry.remove<Component>(m_entity);
		}

		std::string m_name;
		ECS::Entity m_entity;
		Action m_action;
		Component m_value;
	};

} // namespace NeneEngine
