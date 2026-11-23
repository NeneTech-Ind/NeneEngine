// PathResolver.h

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace NeneEngine
{

	[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view path);
	[[nodiscard]] std::string GenericPathToUtf8(const std::filesystem::path& path);
	[[nodiscard]] std::filesystem::path GetExecutableDirectory();
	[[nodiscard]] std::filesystem::path GetEditorLogPath();
	[[nodiscard]] std::filesystem::path GetEditorLayoutPath();
	[[nodiscard]] std::filesystem::path GetEditorThemePath();
	[[nodiscard]] std::filesystem::path ResolveFromAncestors(const std::filesystem::path& start,
	                                                         const std::filesystem::path& relativePath,
	                                                         bool allowMissingLeaf = false);
	[[nodiscard]] std::filesystem::path ResolveFromExecutionRoots(const std::filesystem::path& relativePath,
	                                                              bool allowMissingLeaf = false);

} // namespace NeneEngine
