#include "Core/PathResolver.h"

#include <Windows.h>

#include <format>
#include <system_error>
#include <string>

namespace NeneEngine
{
	namespace
	{
		std::filesystem::path GetEditorDataDirectory()
		{
			const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
			if (size == 0) return {};

			std::wstring value(size, L'\0');
			const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), size);
			if (length == 0 || length >= size) return {};
			value.resize(length);
			const auto directory = std::filesystem::path(value) / "NeneEngine";
			std::error_code error;
			std::filesystem::create_directories(directory, error);
			return error ? std::filesystem::path{} : directory;
		}

		std::filesystem::path MigrateEditorSetting(const std::filesystem::path& directory,
		                                           const std::filesystem::path& filename,
		                                           const std::filesystem::path& legacyPath)
		{
			if (directory.empty()) return legacyPath;
			const auto destination = directory / filename;
			std::error_code error;
			if (std::filesystem::exists(destination, error) || error) return destination;
			if (legacyPath.empty() || !std::filesystem::exists(legacyPath, error) || error) return destination;
			std::filesystem::copy_file(legacyPath, destination, std::filesystem::copy_options::skip_existing, error);
			if (error)
			{
				std::error_code existsError;
				if (!std::filesystem::exists(destination, existsError)) return legacyPath;
			}
			return destination;
		}

		std::filesystem::path FindNearestAncestorMatch(const std::filesystem::path& start,
		                                               const std::filesystem::path& relativePath, bool matchParentOnly)
		{
			std::error_code errorCode;
			auto current = start;
			while (!current.empty())
			{
				const auto candidate = current / relativePath;
				const auto probe = matchParentOnly ? candidate.parent_path() : candidate;
				if (std::filesystem::exists(probe, errorCode)) return candidate;

				const auto parent = current.parent_path();
				if (parent == current) break;
				current = parent;
			}

			return {};
		}
	} // namespace

	std::filesystem::path GetExecutableDirectory()
	{
		wchar_t modulePath[MAX_PATH]{};
		const DWORD pathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (pathLength == 0 || pathLength == MAX_PATH) return {};

		return std::filesystem::path(modulePath).parent_path();
	}

	std::filesystem::path GetEditorLogPath()
	{
		static const std::filesystem::path logPath = []
		{
			SYSTEMTIME now{};
			GetSystemTime(&now);
			FILETIME fileTime{};
			SystemTimeToFileTime(&now, &fileTime);
			const auto ticks = (static_cast<unsigned long long>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
			const auto filename = std::format("nene_engine-{:04}{:02}{:02}-{:02}{:02}{:02}-{}-{}.log", now.wYear,
			                                  now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, ticks,
			                                  GetCurrentProcessId());
			auto root = GetEditorDataDirectory();
			if (root.empty()) root = GetExecutableDirectory();
			if (root.empty()) root = std::filesystem::current_path();
			return root / "logs" / filename;
		}();
		return logPath;
	}

	std::filesystem::path GetEditorLayoutPath()
	{
		return MigrateEditorSetting(GetEditorDataDirectory(), "imgui.ini", GetExecutableDirectory() / "imgui.ini");
	}

	std::filesystem::path GetEditorThemePath()
	{
		const std::filesystem::path relativePath{"assets/config/editor_theme.json"};
		auto legacyPath = ResolveFromExecutionRoots(relativePath, true);
		if (legacyPath.empty()) legacyPath = relativePath;
		return MigrateEditorSetting(GetEditorDataDirectory(), "editor_theme.json", legacyPath);
	}

	std::filesystem::path ResolveFromAncestors(const std::filesystem::path& start,
	                                           const std::filesystem::path& relativePath, bool allowMissingLeaf)
	{
		if (auto existingPath = FindNearestAncestorMatch(start, relativePath, false); !existingPath.empty())
			return existingPath;

		if (!allowMissingLeaf) return {};
		return FindNearestAncestorMatch(start, relativePath, true);
	}

	std::filesystem::path ResolveFromExecutionRoots(const std::filesystem::path& relativePath, bool allowMissingLeaf)
	{
		std::error_code errorCode;
		const std::filesystem::path roots[] = {GetExecutableDirectory(), std::filesystem::current_path(errorCode)};

		for (const auto& root : roots)
		{
			if (root.empty()) continue;
			if (auto existingPath = FindNearestAncestorMatch(root, relativePath, false); !existingPath.empty())
				return existingPath;
		}

		if (!allowMissingLeaf) return {};

		for (const auto& root : roots)
		{
			if (root.empty()) continue;
			if (auto parentMatch = FindNearestAncestorMatch(root, relativePath, true); !parentMatch.empty())
				return parentMatch;
		}

		return {};
	}

} // namespace NeneEngine
