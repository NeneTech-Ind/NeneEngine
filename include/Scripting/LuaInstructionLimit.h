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
		static void DispatchHook(lua_State* state, lua_Debug* frame);
		static void OnLimitReached(lua_State* state, lua_Debug* frame);
		static thread_local LuaInstructionLimit* s_active;

		LuaInstructionLimit* m_parent;

		lua_State* m_state;
		lua_Hook m_previousHook;
		int m_previousMask;
		int m_previousCount;
		lua_Hook m_forwardedHook;
		int m_forwardedMask;
		int m_forwardedCount;
		int m_hookInterval;
		int m_remainingInstructions;
		int m_untilForwardedCount;
	};

} // namespace NeneEngine
