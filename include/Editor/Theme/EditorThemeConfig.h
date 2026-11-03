// EditorThemeConfig.h

#pragma once

#include <imgui.h>
#include <string>

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

	struct EditorThemeMetricsConfig
	{
		ImVec2 windowPadding{10.0f, 10.0f};
		ImVec2 framePadding{8.0f, 5.0f};
		ImVec2 itemSpacing{8.0f, 6.0f};
		float windowRounding = 8.0f;
		float childRounding = 6.0f;
		float popupRounding = 8.0f;
		float frameRounding = 6.0f;
		float tabRounding = 6.0f;
		float grabRounding = 6.0f;
		float scrollbarRounding = 9.0f;
		float windowBorderSize = 1.0f;
		float frameBorderSize = 0.0f;
		float tabBarOverlineSize = 2.0f;
	};

	struct EditorThemeFontsConfig
	{
		std::string uiFontPath = "assets/fonts/nunito_regular.ttf";
		std::string japaneseFontPath = "assets/fonts/m_plus_rounded_1c_regular.ttf";
		std::string monospaceFontPath = "assets/fonts/jetbrains_mono_regular.ttf";
		std::string iconFontPath = "assets/fonts/lucide_icons.ttf";
		float size = 16.0f;
	};

	struct EditorThemeConfig
	{
		EditorThemeColorsConfig colors{};
		EditorThemeMetricsConfig metrics{};
		EditorThemeFontsConfig fonts{};
	};

	[[nodiscard]] EditorThemeConfig GetDefaultEditorThemeConfig();
} // namespace NeneEngine
