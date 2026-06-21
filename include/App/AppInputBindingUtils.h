// AppInputBindingUtils.h

#pragma once

#include "App/AppConfig.h"
#include "Input/InputManager.h"

#include <string_view>

namespace NeneEngine
{

	void ApplyInputBindings(InputManager& inputManager, const InputConfig& inputConfig, std::string_view managerName);

} // namespace NeneEngine
