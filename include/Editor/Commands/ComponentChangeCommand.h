// ComponentChangeCommand.h

#pragma once

#include "ECS/Entity.h"
#include "ECS/World.h"
#include "Editor/Commands/IEditorCommand.h"

#include <string>
#include <utility>

namespace NeneEngine
{
	template <typename Component> class ComponentChangeCommand final : public IEditorCommand
	{
	  public:
		ComponentChangeCommand(std::string name, ECS::Entity entity, Component before, Component after)
		    : m_name(std::move(name)), m_entity(entity), m_before(std::move(before)), m_after(std::move(after))
		{
		}

		[[nodiscard]] const char* GetName() const override { return m_name.c_str(); }

		void Execute(ECS::World& world) override { Apply(world, m_after); }
		void Undo(ECS::World& world) override { Apply(world, m_before); }

	  private:
		void Apply(ECS::World& world, const Component& value) const
		{
			if (!world.GetRegistry().valid(m_entity)) return;

			if (Component* component = world.GetComponent<Component>(m_entity)) *component = value;
		}

		std::string m_name;
		ECS::Entity m_entity;
		Component m_before;
		Component m_after;
	};

} // namespace NeneEngine
