#pragma once

#include <sol/forward.hpp>

namespace NeneEngine
{

	class IInputHandler;

	void BindInputApi(sol::table& nene, const IInputHandler& input);

} // namespace NeneEngine
