// ViewportPanel.h

#pragma once

#include "ECS/Components/TransformComponent.h"
#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"
#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine
{
	class IRenderAdapter;

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
		~ViewportPanel() override;
		[[nodiscard]] const char* GetName() const override { return "Viewport"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_VIEWPORT; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Center; }
		[[nodiscard]] ImGuiWindowFlags GetWindowFlags() const override
		{
			return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		}

		void Draw(EditorContext& context) override;
		void OnSceneLoaded() override
		{
			m_wasUsingGizmo = false;
			m_dragEntity = ECS::NullEntity;
		}

	  private:
		void HandleShortcuts(const EditorContext& context);
		void DrawToolbar();
		void DrawGizmo(EditorContext& context);
		void TrackGizmoUndo(EditorContext& context, bool isUsing);

		GizmoOperation m_operation = GizmoOperation::Translate;
		GizmoSpace m_space = GizmoSpace::World;
		bool m_useSnap = false;

		bool m_wasUsingGizmo = false;
		ECS::Entity m_dragEntity = ECS::NullEntity;
		ECS::TransformComponent m_transformBeforeDrag{};
		IRenderAdapter* m_renderer = nullptr;
		RenderTarget m_target{};
		ImVec2 m_imagePosition{};
		ImVec2 m_imageSize{};
	};

} // namespace NeneEngine
