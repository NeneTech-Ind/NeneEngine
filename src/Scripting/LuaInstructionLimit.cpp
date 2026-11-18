#include "Scripting/LuaInstructionLimit.h"

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kLimitMessage = "Lua execution exceeded its instruction limit";
	}

	LuaInstructionLimit::LuaInstructionLimit(lua_State* state, int instructionCount)
	    : m_state(state), m_previousHook(lua_gethook(state)), m_previousMask(lua_gethookmask(state)),
	      m_previousCount(lua_gethookcount(state))
	{
		lua_sethook(m_state, OnLimitReached, LUA_MASKCOUNT, instructionCount);
	}

	LuaInstructionLimit::~LuaInstructionLimit()
	{
		lua_sethook(m_state, m_previousHook, m_previousMask, m_previousCount);
	}

	void LuaInstructionLimit::OnLimitReached(lua_State* state, lua_Debug* frame)
	{
		// Keep the budget exhausted until the engine's protected call returns, even if Lua catches the error
		lua_sethook(state, OnLimitReached, LUA_MASKCOUNT, 1);
		if (lua_getinfo(state, "Sl", frame) != 0 && frame->currentline > 0)
		{
			const char* source =
			    frame->source != nullptr && frame->source[0] == '@' ? frame->source + 1 : frame->short_src;
			lua_pushfstring(state, "%s:%d: %s", source, frame->currentline, kLimitMessage);
			lua_error(state);
			return;
		}
		luaL_error(state, "%s", kLimitMessage);
	}

} // namespace NeneEngine
