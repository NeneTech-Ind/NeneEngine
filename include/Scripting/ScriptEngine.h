#pragma once

#include "Scripting/ScriptFieldSchema.h"

#include <filesystem>
#include <memory>
#include <sol/forward.hpp>

namespace NeneEngine
{

	class ScriptEngine final
	{
	  public:
		ScriptEngine();
		~ScriptEngine();

		void Initialize();
		void Shutdown();

		// Load paths relative to assets/scripts and cache successful loads
		// Returned pointers are valid until Shutdown()
		[[nodiscard]] const sol::table* LoadScript(const std::filesystem::path& path);

		// Returns nullptr if the script has not been loaded successfully
		[[nodiscard]] const ScriptFieldSchema* GetFieldSchema(const std::filesystem::path& path) const;

	  private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
