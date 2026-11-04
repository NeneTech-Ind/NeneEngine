// EditorThemeLoader.h

#pragma once

#include "Editor/Theme/EditorThemeConfig.h"

#include <filesystem>

namespace NeneEngine
{
	[[nodiscard]] std::filesystem::path GetDefaultEditorThemeConfigPath();
	[[nodiscard]] EditorThemeVariant LoadEditorThemeVariant(const std::filesystem::path& configPath);
	void SaveEditorThemeVariant(const std::filesystem::path& configPath, EditorThemeVariant variant);
} // namespace NeneEngine
