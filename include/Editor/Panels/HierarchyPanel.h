// HierarchyPanel.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"

#include <EASTL/vector.h>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class HierarchyPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Scene Hierarchy"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Left; }

		void Draw(EditorContext& context) override;

	  private:
		void CollectRoots(const ECS::World& world);
		void UpdateRevealPath(const EditorContext& context);
		void DrawEntityTree(EditorContext& context, ECS::Entity entity, int depth);
		void DrawFilteredList(EditorContext& context);

		ImGuiTextFilter m_filter;
		eastl::vector<ECS::Entity> m_roots;
		eastl::vector<ECS::Entity> m_matches;

		ECS::Entity m_revealedEntity = ECS::NullEntity;
		eastl::vector<ECS::Entity> m_revealPath;
	};

} // namespace NeneEngine
