#pragma once

#include "Scripting/ScriptFieldSchema.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sol/forward.hpp>
#include <string_view>

namespace NeneEngine
{

	class IInputHandler;
	class PrefabLibrary;
	class ResourceLoadQueue;

	namespace ECS
	{
		struct ScriptComponent;
		class World;
	}

	class ScriptEngine final
	{
	  public:
		ScriptEngine();
		~ScriptEngine();

		ScriptEngine(const ScriptEngine&) = delete;
		ScriptEngine& operator=(const ScriptEngine&) = delete;

		// The input handler must stay alive until Shutdown()
		void Initialize(const IInputHandler& input, ECS::World& world, PrefabLibrary& prefabs, ResourceLoadQueue& loads);
		void Shutdown();

		void ExecuteConsoleCommand(std::string_view command, const ECS::ScriptComponent* selectedScript);

		// Load paths relative to assets/scripts and cache the result of each attempt
		// Reloads update the cached class and schema in place
		[[nodiscard]] const sol::table* LoadScript(const std::filesystem::path& path);

		// Queue reloads on the main thread
		void ReloadScript(const std::filesystem::path& path);
		void ReloadAll();
		void ReloadChangedScripts();
		void PollScriptChanges();
		[[nodiscard]] bool IsReloadPending() const;

		// Returns a tracked path with a load error, including failed reloads that kept the last working version
		[[nodiscard]] const std::filesystem::path* FindFailedScript() const;

		// Zero means no successful load. Failed reloads keep the last working revision.
		[[nodiscard]] std::uint64_t GetScriptRevision(const std::filesystem::path& path) const;

		// Changes when a script revision is successfully published, including its first load.
		[[nodiscard]] std::uint64_t GetScriptCacheGeneration() const;

		// Returns nullptr if the script has not been loaded successfully
		[[nodiscard]] const ScriptFieldSchema* GetFieldSchema(const std::filesystem::path& path) const;

	  private:
		struct Impl;
		std::shared_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
