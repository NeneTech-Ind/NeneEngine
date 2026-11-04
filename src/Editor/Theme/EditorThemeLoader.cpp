// EditorThemeLoader.cpp

#include "Editor/Theme/EditorThemeLoader.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kThemeKey = "theme";
		constexpr const char* kEveningName = "evening";
		constexpr const char* kDayName = "day";

		constexpr const char* kRelativeConfigPath = "assets/config/editor_theme.json";

		std::string ToUtf8(const std::filesystem::path& path)
		{
			const std::u8string utf8Path = path.u8string();
			return std::string(utf8Path.begin(), utf8Path.end());
		}

		const char* ToThemeName(EditorThemeVariant variant)
		{
			return variant == EditorThemeVariant::Day ? kDayName : kEveningName;
		}

		nlohmann::json ReadJsonOrEmpty(const std::filesystem::path& configPath)
		{
			std::ifstream input(configPath);
			if (!input.is_open()) return nlohmann::json::object();

			try
			{
				nlohmann::json root;
				input >> root;
				if (root.is_object()) return root;
				NENE_LOG_WARN("Editor theme config: '{}' is not a JSON object", ToUtf8(configPath));
			}
			catch (const std::exception& ex)
			{
				NENE_LOG_WARN("Editor theme config: failed to parse '{}': {}", ToUtf8(configPath), ex.what());
			}
			return nlohmann::json::object();
		}
	} // namespace

	std::filesystem::path GetDefaultEditorThemeConfigPath()
	{
		if (std::filesystem::path resolvedPath = ResolveFromExecutionRoots(kRelativeConfigPath, true);
		    !resolvedPath.empty())
			return resolvedPath;

		return std::filesystem::path{kRelativeConfigPath};
	}

	EditorThemeVariant LoadEditorThemeVariant(const std::filesystem::path& configPath)
	{
		const nlohmann::json root = ReadJsonOrEmpty(configPath);
		const auto theme = root.find(kThemeKey);
		if (theme == root.end() || !theme->is_string()) return EditorThemeVariant::Evening;

		const std::string name = theme->get<std::string>();
		if (name == kDayName) return EditorThemeVariant::Day;
		if (name != kEveningName) NENE_LOG_WARN("Editor theme config: unknown theme '{}'", name);
		return EditorThemeVariant::Evening;
	}

	void SaveEditorThemeVariant(const std::filesystem::path& configPath, EditorThemeVariant variant)
	{
		nlohmann::json root = ReadJsonOrEmpty(configPath);
		root[kThemeKey] = ToThemeName(variant);

		std::ofstream output(configPath, std::ios::trunc);
		if (!output.is_open())
		{
			NENE_LOG_WARN("Editor theme config: failed to write '{}'", ToUtf8(configPath));
			return;
		}
		output << root.dump(4) << '\n';
	}
} // namespace NeneEngine
