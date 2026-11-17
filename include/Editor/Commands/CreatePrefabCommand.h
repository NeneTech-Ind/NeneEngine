#pragma once

#include "Editor/Commands/IEditorCommand.h"
#include "Scene/EntitySerializer.h"

namespace NeneEngine
{

	class CreatePrefabCommand final : public IEditorCommand
	{
	  public:
		CreatePrefabCommand(const ECS::World& world, ECS::Entity root);

		[[nodiscard]] const char* GetName() const override { return "Create Prefab"; }
		void Execute(ECS::World& world) override;
		void Undo(ECS::World& world) override;

	  private:
		ECS::Entity m_root;
		EntityDeserializationMap m_entities;
		nlohmann::json m_data;
	};

}
