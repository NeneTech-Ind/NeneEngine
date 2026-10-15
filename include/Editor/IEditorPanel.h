// IEditorPanel.h

#pragma once

#include <imgui.h>

namespace NeneEngine
{
	struct EditorContext;

	enum class EditorDockSlot
	{
		Left,
		Right,
		Bottom,
		Center
	};

	class IEditorPanel
	{
	  public:
		virtual ~IEditorPanel() = default;

		[[nodiscard]] virtual const char* GetName() const = 0;
		[[nodiscard]] virtual EditorDockSlot GetDefaultDockSlot() const = 0;
		[[nodiscard]] virtual ImGuiWindowFlags GetWindowFlags() const { return ImGuiWindowFlags_None; }

		virtual void Draw(EditorContext& context) = 0;
	};

} // namespace NeneEngine
