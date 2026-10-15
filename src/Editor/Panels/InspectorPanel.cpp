// InspectorPanel.cpp

#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorContext.h"

#include <imgui.h>

namespace NeneEngine
{

	void InspectorPanel::Draw(EditorContext& /*context*/)
	{
		ImGui::TextDisabled("Select an entity to inspect it");
	}

} // namespace NeneEngine
