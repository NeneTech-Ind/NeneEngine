// HierarchyPanel.cpp

#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorContext.h"

#include <imgui.h>

namespace NeneEngine
{

	void HierarchyPanel::Draw(EditorContext& /*context*/)
	{
		ImGui::TextDisabled("No entities listed yet");
	}

} // namespace NeneEngine
