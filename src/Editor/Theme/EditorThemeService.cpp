// EditorThemeService.cpp

#include "Editor/Theme/EditorThemeService.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"

#include <filesystem>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr float kHoveredAlpha = 0.35f;
		constexpr float kActiveAlpha = 0.55f;
		constexpr float kSelectedAlpha = 0.25f;
		constexpr float kFrameHoveredAlpha = 0.18f;
		constexpr float kFrameActiveAlpha = 0.28f;

		ImVec4 WithAlpha(const ImVec4& color, float alpha)
		{
			return ImVec4(color.x, color.y, color.z, alpha);
		}

		std::filesystem::path FindFontFile(const std::string& relativePath)
		{
			std::filesystem::path path = ResolveFromExecutionRoots(relativePath);
			if (path.empty()) NENE_LOG_WARN("Editor font not found: {}", relativePath);
			return path;
		}

		ImFont* AddFont(const std::filesystem::path& path, float size, bool mergeIntoPrevious)
		{
			ImFontConfig fontConfig;
			fontConfig.MergeMode = mergeIntoPrevious;
			fontConfig.Flags |= ImFontFlags_NoLoadError;

			const std::u8string utf8Path = path.u8string();
			ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8Path.c_str()),
			                                                         size, &fontConfig);
			if (font == nullptr)
				NENE_LOG_WARN("Editor font failed to load: {}", reinterpret_cast<const char*>(utf8Path.c_str()));
			return font;
		}
	} // namespace

	void EditorThemeService::Apply(const EditorThemeConfig& config)
	{
		m_config = config;
		ApplyColors();
		ApplyMetrics();
	}

	void EditorThemeService::LoadFonts()
	{
		if (m_uiFont != nullptr) return;

		const EditorThemeFontsConfig& fonts = m_config.fonts;
		const std::filesystem::path uiFontPath = FindFontFile(fonts.uiFontPath);
		if (!uiFontPath.empty()) m_uiFont = AddFont(uiFontPath, fonts.size, false);
		if (m_uiFont == nullptr)
		{
			NENE_LOG_WARN("Editor uses the default ImGui font");
			return;
		}

		if (const std::filesystem::path japaneseFontPath = FindFontFile(fonts.japaneseFontPath);
		    !japaneseFontPath.empty())
			AddFont(japaneseFontPath, fonts.size, true);
		ImGui::GetIO().FontDefault = m_uiFont;

		if (const std::filesystem::path monospaceFontPath = FindFontFile(fonts.monospaceFontPath);
		    !monospaceFontPath.empty())
			m_monospaceFont = AddFont(monospaceFontPath, fonts.size, false);
	}

	void EditorThemeService::ApplyColors() const
	{
		const EditorThemeColorsConfig& theme = m_config.colors;
		const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
		ImVec4* colors = ImGui::GetStyle().Colors;

		colors[ImGuiCol_Text] = theme.text;
		colors[ImGuiCol_TextDisabled] = theme.textMuted;
		colors[ImGuiCol_WindowBg] = theme.background;
		colors[ImGuiCol_ChildBg] = transparent;
		colors[ImGuiCol_PopupBg] = theme.backgroundRaised;
		colors[ImGuiCol_Border] = theme.border;
		colors[ImGuiCol_BorderShadow] = transparent;

		colors[ImGuiCol_FrameBg] = theme.backgroundSunken;
		colors[ImGuiCol_FrameBgHovered] = WithAlpha(theme.accent, kFrameHoveredAlpha);
		colors[ImGuiCol_FrameBgActive] = WithAlpha(theme.accent, kFrameActiveAlpha);

		colors[ImGuiCol_TitleBg] = theme.backgroundSunken;
		colors[ImGuiCol_TitleBgActive] = theme.backgroundSunken;
		colors[ImGuiCol_TitleBgCollapsed] = theme.backgroundSunken;
		colors[ImGuiCol_MenuBarBg] = theme.backgroundRaised;

		colors[ImGuiCol_ScrollbarBg] = theme.backgroundSunken;
		colors[ImGuiCol_ScrollbarGrab] = theme.border;
		colors[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(theme.accent, kHoveredAlpha);
		colors[ImGuiCol_ScrollbarGrabActive] = WithAlpha(theme.accent, kActiveAlpha);

		colors[ImGuiCol_CheckMark] = theme.accent;
		colors[ImGuiCol_SliderGrab] = WithAlpha(theme.accent, 0.8f);
		colors[ImGuiCol_SliderGrabActive] = theme.accent;

		colors[ImGuiCol_Button] = theme.border;
		colors[ImGuiCol_ButtonHovered] = WithAlpha(theme.accent, kHoveredAlpha);
		colors[ImGuiCol_ButtonActive] = WithAlpha(theme.accent, kActiveAlpha);

		colors[ImGuiCol_Header] = WithAlpha(theme.accent, kSelectedAlpha);
		colors[ImGuiCol_HeaderHovered] = WithAlpha(theme.accent, kHoveredAlpha);
		colors[ImGuiCol_HeaderActive] = WithAlpha(theme.accent, kActiveAlpha);

		colors[ImGuiCol_Separator] = theme.border;
		colors[ImGuiCol_SeparatorHovered] = WithAlpha(theme.accent, kActiveAlpha);
		colors[ImGuiCol_SeparatorActive] = theme.accent;

		colors[ImGuiCol_ResizeGrip] = WithAlpha(theme.accent, kFrameHoveredAlpha);
		colors[ImGuiCol_ResizeGripHovered] = WithAlpha(theme.accent, kActiveAlpha);
		colors[ImGuiCol_ResizeGripActive] = theme.accent;

		colors[ImGuiCol_InputTextCursor] = theme.text;

		colors[ImGuiCol_Tab] = theme.backgroundSunken;
		colors[ImGuiCol_TabHovered] = WithAlpha(theme.accent, kHoveredAlpha);
		colors[ImGuiCol_TabSelected] = theme.background;
		colors[ImGuiCol_TabSelectedOverline] = theme.accent;
		colors[ImGuiCol_TabDimmed] = theme.backgroundSunken;
		colors[ImGuiCol_TabDimmedSelected] = theme.background;
		colors[ImGuiCol_TabDimmedSelectedOverline] = theme.border;

		colors[ImGuiCol_DockingPreview] = WithAlpha(theme.accent, kActiveAlpha);
		colors[ImGuiCol_DockingEmptyBg] = theme.background;

		colors[ImGuiCol_PlotLines] = theme.textMuted;
		colors[ImGuiCol_PlotLinesHovered] = theme.accentPlay;
		colors[ImGuiCol_PlotHistogram] = theme.accent;
		colors[ImGuiCol_PlotHistogramHovered] = theme.accentPlay;

		colors[ImGuiCol_TableHeaderBg] = theme.backgroundRaised;
		colors[ImGuiCol_TableBorderStrong] = theme.border;
		colors[ImGuiCol_TableBorderLight] = WithAlpha(theme.border, 0.6f);
		colors[ImGuiCol_TableRowBg] = transparent;
		colors[ImGuiCol_TableRowBgAlt] = WithAlpha(theme.text, 0.04f);

		colors[ImGuiCol_TextLink] = theme.accent;
		colors[ImGuiCol_TextSelectedBg] = WithAlpha(theme.accent, kHoveredAlpha);
		colors[ImGuiCol_TreeLines] = theme.border;
		colors[ImGuiCol_DragDropTarget] = theme.accentPlay;

		colors[ImGuiCol_NavCursor] = theme.accent;
		colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(theme.text, 0.7f);
		colors[ImGuiCol_NavWindowingDimBg] = WithAlpha(theme.backgroundSunken, 0.6f);
		colors[ImGuiCol_ModalWindowDimBg] = WithAlpha(theme.backgroundSunken, 0.6f);
	}

	void EditorThemeService::ApplyMetrics() const
	{
		const EditorThemeMetricsConfig& metrics = m_config.metrics;
		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowPadding = metrics.windowPadding;
		style.FramePadding = metrics.framePadding;
		style.ItemSpacing = metrics.itemSpacing;
		style.WindowRounding = metrics.windowRounding;
		style.ChildRounding = metrics.childRounding;
		style.PopupRounding = metrics.popupRounding;
		style.FrameRounding = metrics.frameRounding;
		style.TabRounding = metrics.tabRounding;
		style.GrabRounding = metrics.grabRounding;
		style.ScrollbarRounding = metrics.scrollbarRounding;
		style.WindowBorderSize = metrics.windowBorderSize;
		style.FrameBorderSize = metrics.frameBorderSize;
		style.TabBarOverlineSize = metrics.tabBarOverlineSize;
	}

} // namespace NeneEngine
