// EditorThemeConfig.h

#pragma once

#include <imgui.h>

namespace NeneEngine
{
	struct EditorThemeColorsConfig
	{
		ImVec4 background{};
		ImVec4 backgroundRaised{};
		ImVec4 backgroundSunken{};
		ImVec4 border{};
		ImVec4 text{};
		ImVec4 textMuted{};
		ImVec4 accent{};
		ImVec4 accentPlay{};
		ImVec4 success{};
		ImVec4 danger{};
		ImVec4 axisX{};
		ImVec4 axisY{};
		ImVec4 axisZ{};
	};

	struct EditorThemeConfig
	{
		EditorThemeColorsConfig colors{};
	};

	[[nodiscard]] EditorThemeConfig GetDefaultEditorThemeConfig();
} // namespace NeneEngine
