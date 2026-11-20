#include "Scripting/Bindings/InputBindings.h"

#include "Input/IInputHandler.h"
#include "Input/KeyCodeStrings.h"

#include <sol/sol.hpp>
#include <string_view>

namespace NeneEngine
{

	void BindInputApi(sol::table& nene, const IInputHandler& input)
	{
		auto inputApi = nene.create_named("input");
		inputApi.set_function("is_action_down",
		                      [&input](std::string_view action) { return input.IsActionActive(action); });
		inputApi.set_function("is_action_pressed",
		                      [&input](std::string_view action) { return input.IsActionPressed(action); });
		inputApi.set_function("is_key_down",
		                      [&input](std::string_view key)
		                      {
			                      const auto keyCode = TryParseKeyCode(key);
			                      return keyCode && input.IsKeyDown(*keyCode);
		                      });
	}

} // namespace NeneEngine
