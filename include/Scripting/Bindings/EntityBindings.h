#pragma once

#include <sol/forward.hpp>

namespace NeneEngine
{

	class ScriptExecutionContext;

	void BindEntityApi(sol::table& nene, const ScriptExecutionContext& execution);

}
