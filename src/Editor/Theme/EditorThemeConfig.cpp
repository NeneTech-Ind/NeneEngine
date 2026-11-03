// EditorThemeConfig.cpp

#include "Editor/Theme/EditorThemeConfig.h"

#include <cstdint>

namespace NeneEngine
{
	namespace
	{
		constexpr ImVec4 ToImVec4(uint32_t rgb)
		{
			return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
			              static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f,
			              1.0f);
		}
	} // namespace

	EditorThemeConfig GetDefaultEditorThemeConfig()
	{
		EditorThemeConfig config{};
		EditorThemeColorsConfig& colors = config.colors;
		colors.background = ToImVec4(0x1E1A24);
		colors.backgroundRaised = ToImVec4(0x28222F);
		colors.backgroundSunken = ToImVec4(0x16131B);
		colors.border = ToImVec4(0x3D3445);
		colors.text = ToImVec4(0xF4EDF2);
		colors.textMuted = ToImVec4(0xA99BAE);
		colors.accent = ToImVec4(0xFF8FB1);
		colors.accentPlay = ToImVec4(0xFFB37A);
		colors.success = ToImVec4(0x7FD8BE);
		colors.danger = ToImVec4(0xFF6B7A);
		colors.axisX = ToImVec4(0xFF7A8A);
		colors.axisY = ToImVec4(0x8FE3A0);
		colors.axisZ = ToImVec4(0x8EC5FF);
		return config;
	}
} // namespace NeneEngine
