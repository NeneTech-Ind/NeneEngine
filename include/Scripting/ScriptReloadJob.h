#pragma once

#include <EASTL/optional.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace NeneEngine
{
	using ScriptWriteTime = eastl::optional<std::filesystem::file_time_type>;

	struct ScriptReloadOptions
	{
		bool forceAll = false;
		std::vector<std::filesystem::path> targets;
	};

	struct TrackedScriptSource
	{
		std::filesystem::path path;
		ScriptWriteTime lastAttemptedWriteTime;
		bool usesRequire = false;
		bool retryOnModuleReload = false;
	};

	struct TrackedScriptModule
	{
		std::filesystem::path path;
		ScriptWriteTime lastAttemptedWriteTime;
	};

	struct ScriptReloadRequest
	{
		std::filesystem::path scriptsRoot;
		std::string moduleSearchPath;
		ScriptReloadOptions options;
		std::vector<TrackedScriptSource> scripts;
		std::unordered_map<std::string, TrackedScriptModule> modules;
		bool retryModuleReload = false;
	};

	struct PreparedScriptChunk
	{
		std::filesystem::path path;
		ScriptWriteTime writeTime;
		std::vector<char> bytecode;
		std::string error;
	};

	struct ScriptReloadResult
	{
		std::unordered_map<std::filesystem::path, PreparedScriptChunk> scripts;
		std::unordered_map<std::string, PreparedScriptChunk> modules;
		bool reloadModules = false;
		bool sourcesChangedDuringRead = false;
	};

	[[nodiscard]] ScriptWriteTime ReadScriptWriteTime(const std::filesystem::path& path);
	[[nodiscard]] ScriptReloadResult PrepareScriptReload(const ScriptReloadRequest& request);

} // namespace NeneEngine
