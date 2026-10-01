// KeyCodeStrings.h

#pragma once

#include "Input/KeyCode.h"

#include <EASTL/optional.h>
#include <string_view>

namespace NeneEngine
{

	[[nodiscard]] std::string_view ToString(KeyCode keyCode);
	[[nodiscard]] eastl::optional<KeyCode> TryParseKeyCode(std::string_view value);

} // namespace NeneEngine
