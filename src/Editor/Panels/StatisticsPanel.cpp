// StatisticsPanel.cpp

#include "Editor/Panels/StatisticsPanel.h"

#include "Editor/EditorContext.h"

#include <imgui.h>

namespace NeneEngine
{

	void StatisticsPanel::Draw(EditorContext& /*context*/)
	{
		ImGui::TextDisabled("No statistics collected yet");
	}

} // namespace NeneEngine
