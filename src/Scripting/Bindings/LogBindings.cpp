#include "Scripting/Bindings/LogBindings.h"

#include "Core/NeneLogger.h"

#include <format>
#include <sol/sol.hpp>
#include <string>
#include <string_view>

namespace NeneEngine
{
	namespace
	{
		std::string FormatScriptLogMessage(lua_State* luaState, std::string_view message)
		{
			lua_Debug frame{};
			for (int level = 1; lua_getstack(luaState, level, &frame) != 0; ++level)
			{
				if (lua_getinfo(luaState, "Sl", &frame) == 0 || frame.currentline <= 0 || frame.source == nullptr)
					continue;

				const std::string_view source{frame.source, frame.srclen};
				if (source.starts_with('@'))
					return std::format("[{}:{}] {}", source.substr(1), frame.currentline, message);
				break;
			}

			return std::format("[Lua] {}", message);
		}
	}

	void BindLogApi(sol::table& nene)
	{
		auto logApi = nene.create_named("log");
		logApi.set_function("info", [](sol::this_state state, std::string_view message)
		                    { NENE_LOG_INFO("{}", FormatScriptLogMessage(state, message)); });
		logApi.set_function("warn", [](sol::this_state state, std::string_view message)
		                    { NENE_LOG_WARN("{}", FormatScriptLogMessage(state, message)); });
		logApi.set_function("error", [](sol::this_state state, std::string_view message)
		                    { NENE_LOG_ERROR("{}", FormatScriptLogMessage(state, message)); });
	}

} // namespace NeneEngine
