#include "Core/PathResolver.h"

#include <Windows.h>

#include <system_error>

namespace NeneEngine
{
	namespace
	{
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
