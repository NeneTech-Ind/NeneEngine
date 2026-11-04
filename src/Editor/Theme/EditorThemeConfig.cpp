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

	EditorThemeColorsConfig GetEditorThemeColors(EditorThemeVariant variant)
	{
		EditorThemeColorsConfig colors{};
		if (variant == EditorThemeVariant::Day)
		{
			colors.background = ToImVec4(0xFFF7F9);
			colors.backgroundRaised = ToImVec4(0xFFFFFF);
			colors.backgroundSunken = ToImVec4(0xFBE8EE);
			colors.border = ToImVec4(0xEBCAD5);
			colors.text = ToImVec4(0x3A2E39);
			colors.textMuted = ToImVec4(0x74606F);
			colors.accent = ToImVec4(0x6B3FC4);
			colors.accentPlay = ToImVec4(0xA34E0C);
			colors.success = ToImVec4(0x17795D);
			colors.danger = ToImVec4(0xC42F43);
			colors.axisX = ToImVec4(0xC42F43);
			colors.axisY = ToImVec4(0x17795D);
			colors.axisZ = ToImVec4(0x2B63B8);
			return colors;
		}

		colors.background = ToImVec4(0x1E1A24);
		colors.backgroundRaised = ToImVec4(0x28222F);
		colors.backgroundSunken = ToImVec4(0x16131B);
		colors.border = ToImVec4(0x3D3445);
		colors.text = ToImVec4(0xF4EDF2);
		colors.textMuted = ToImVec4(0xA99BAE);
		colors.accent = ToImVec4(0xB79CFF);
		colors.accentPlay = ToImVec4(0xFFB37A);
		colors.success = ToImVec4(0x7FD8BE);
		colors.danger = ToImVec4(0xFF6B7A);
		colors.axisX = ToImVec4(0xFF7A8A);
		colors.axisY = ToImVec4(0x8FE3A0);
		colors.axisZ = ToImVec4(0x8EC5FF);
		return colors;
	}

	EditorThemeConfig GetDefaultEditorThemeConfig(EditorThemeVariant variant)
	{
		EditorThemeConfig config{};
		config.variant = variant;
		config.colors = GetEditorThemeColors(variant);
		return config;
	}
} // namespace NeneEngine
