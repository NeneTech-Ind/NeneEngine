#include "Scripting/LuaInstructionLimit.h"

#include <numeric>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kLimitMessage = "Lua execution exceeded its instruction limit";

		int EventMask(int event)
		{
			switch (event)
			{
			case LUA_HOOKCALL:
			case LUA_HOOKTAILCALL: return LUA_MASKCALL;
			case LUA_HOOKRET: return LUA_MASKRET;
			case LUA_HOOKLINE: return LUA_MASKLINE;
			case LUA_HOOKCOUNT: return LUA_MASKCOUNT;
			default: return 0;
			}
		}
	}

	thread_local LuaInstructionLimit* LuaInstructionLimit::s_active = nullptr;

	LuaInstructionLimit::LuaInstructionLimit(lua_State* state, int instructionCount)
	    : m_parent(s_active), m_state(state), m_previousHook(lua_gethook(state)),
	      m_previousMask(lua_gethookmask(state)), m_previousCount(lua_gethookcount(state)),
	      m_forwardedHook(m_previousHook), m_forwardedMask(m_previousMask), m_forwardedCount(m_previousCount),
	      m_hookInterval(instructionCount), m_remainingInstructions(instructionCount), m_untilForwardedCount(0)
	{
		if (m_previousHook == DispatchHook || m_previousHook == OnLimitReached)
		{
			auto* scope = m_parent;
			while (scope && scope->m_state != state) scope = scope->m_parent;
			m_forwardedHook = scope ? scope->m_forwardedHook : nullptr;
			m_forwardedMask = scope ? scope->m_forwardedMask : 0;
			m_forwardedCount = scope ? scope->m_forwardedCount : 0;
		}
		if ((m_forwardedMask & LUA_MASKCOUNT) && m_forwardedCount > 0)
			m_hookInterval = std::gcd(instructionCount, m_forwardedCount);
		m_untilForwardedCount = m_forwardedCount;
		s_active = this;
		lua_sethook(m_state, m_forwardedHook ? DispatchHook : OnLimitReached,
		            m_forwardedMask | LUA_MASKCOUNT, m_hookInterval);
	}

	LuaInstructionLimit::~LuaInstructionLimit()
	{
		s_active = m_parent;
		lua_sethook(m_state, m_previousHook, m_previousMask, m_previousCount);
	}

	void LuaInstructionLimit::DispatchHook(lua_State* state, lua_Debug* frame)
	{
		auto* scope = s_active;
		while (scope && scope->m_state != state) scope = scope->m_parent;
		if (!scope) return;

		if (frame->event == LUA_HOOKCOUNT)
		{
			scope->m_remainingInstructions -= scope->m_hookInterval;
			if (scope->m_remainingInstructions <= 0)
			{
				OnLimitReached(state, frame);
				return;
			}
			if (!(scope->m_forwardedMask & LUA_MASKCOUNT) || scope->m_forwardedCount <= 0) return;
			scope->m_untilForwardedCount -= scope->m_hookInterval;
			if (scope->m_untilForwardedCount > 0) return;
			scope->m_untilForwardedCount = scope->m_forwardedCount;
		}
		if (scope->m_forwardedHook && (scope->m_forwardedMask & EventMask(frame->event)))
			scope->m_forwardedHook(state, frame);
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
