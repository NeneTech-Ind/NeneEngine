#pragma once

#include <lua.hpp>

namespace NeneEngine
{

	class LuaInstructionLimit final
	{
	  public:
		LuaInstructionLimit(lua_State* state, int instructionCount);
		~LuaInstructionLimit();

		LuaInstructionLimit(const LuaInstructionLimit&) = delete;
		LuaInstructionLimit& operator=(const LuaInstructionLimit&) = delete;

	  private:
		static void OnLimitReached(lua_State* state, lua_Debug* frame);

		lua_State* m_state;
		lua_Hook m_previousHook;
		int m_previousMask;
		int m_previousCount;
	};

} // namespace NeneEngine
