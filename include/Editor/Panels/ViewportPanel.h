// ViewportPanel.h

#pragma once

#include "ECS/Components/TransformComponent.h"
#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"

namespace NeneEngine
{

	enum class GizmoOperation
	{
		Translate,
		Rotate,
		Scale
	};

	enum class GizmoSpace
	{
		Local,
		World
	};

	class ViewportPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Viewport"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Center; }
		[[nodiscard]] ImGuiWindowFlags GetWindowFlags() const override
		{
			return ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			       ImGuiWindowFlags_NoMove;
		}

		void Draw(EditorContext& context) override;

	  private:
		void HandleShortcuts(const EditorContext& context);
		void DrawToolbar();
		void DrawGizmo(EditorContext& context);
		void TrackGizmoUndo(EditorContext& context, ECS::Entity entity, bool isUsing);

		GizmoOperation m_operation = GizmoOperation::Translate;
		GizmoSpace m_space = GizmoSpace::World;
		bool m_useSnap = false;

		bool m_wasUsingGizmo = false;
		ECS::Entity m_dragEntity = ECS::NullEntity;
		ECS::TransformComponent m_transformBeforeDrag{};
	};

} // namespace NeneEngine
