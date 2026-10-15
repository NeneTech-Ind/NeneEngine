// ViewportPanel.h

#pragma once

#include "Editor/IEditorPanel.h"

namespace NeneEngine
{

	class ViewportPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Viewport"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Center; }
		[[nodiscard]] ImGuiWindowFlags GetWindowFlags() const override
		{
			return ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		}

		void Draw(EditorContext& context) override;
	};

} // namespace NeneEngine
