// StatisticsPanel.cpp

#include "Editor/Panels/StatisticsPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorWidgets.h"

#include <imgui.h>

namespace NeneEngine
{

	void StatisticsPanel::Draw(EditorContext& context)
	{
		DrawEmptyState(context.GetMascot(EditorMascotPose::Sleep), "No statistics collected yet");
	}

} // namespace NeneEngine
