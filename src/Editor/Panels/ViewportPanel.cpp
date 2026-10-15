// ViewportPanel.cpp

#include "Editor/Panels/ViewportPanel.h"

#include "Editor/EditorContext.h"

#include <imgui.h>

namespace NeneEngine
{

	void ViewportPanel::Draw(EditorContext& context)
	{
		context.isViewportHovered = ImGui::IsWindowHovered();
		context.isViewportFocused = ImGui::IsWindowFocused();
	}

} // namespace NeneEngine
