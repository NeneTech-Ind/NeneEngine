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

		// The input handler must stay alive until Shutdown()
		void Initialize(const IInputHandler& input, ECS::World& world, PrefabLibrary& prefabs);
		void Shutdown();

		void ExecuteConsoleCommand(std::string_view command, const ECS::ScriptComponent* selectedScript);

		// Load paths relative to assets/scripts and cache the result of each attempt
		// Reloads update the cached class and schema in place
		[[nodiscard]] const sol::table* LoadScript(const std::filesystem::path& path);

		// Call on the main thread in Edit mode
		void ReloadScript(const std::filesystem::path& path);
		void ReloadAll();
		// Also safe between world updates in Play mode; updated behaviours restart on the next update
		void ReloadChangedScripts();

		// Includes failed attempts that kept the last working version
		[[nodiscard]] bool HasScriptLoadErrors() const;

		// Zero means no successful load. Failed reloads keep the last working revision.
		[[nodiscard]] std::uint64_t GetScriptRevision(const std::filesystem::path& path) const;

		// Returns nullptr if the script has not been loaded successfully
		[[nodiscard]] const ScriptFieldSchema* GetFieldSchema(const std::filesystem::path& path) const;

	  private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
