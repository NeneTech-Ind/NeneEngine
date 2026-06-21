// AppInputBindingUtils.cpp

#include "App/AppInputBindingUtils.h"

#include "Core/CustomLogger.h"
#include "Input/KeyCodeStrings.h"

#include <sstream>

namespace NeneEngine
{
	namespace
	{
		std::string FormatBindings(const std::vector<KeyCode>& keyCodes)
		{
			std::ostringstream stream;
			for (size_t index = 0; index < keyCodes.size(); ++index)
			{
				if (index > 0) stream << ", ";
				stream << ToString(keyCodes[index]);
			}

			return stream.str();
		}

		void LogAppliedInputBindings(const InputManager& inputManager, std::string_view managerName)
		{
			const auto& bindings = inputManager.GetActionBindings();
			NENE_LOG_INFO("Applied input bindings to {}: {} actions", managerName, bindings.size());
			for (const auto& [actionName, keyCodes] : bindings)
			{
				NENE_LOG_INFO("  {} -> [{}]", actionName, FormatBindings(keyCodes));
			}
		}
	} // namespace

	void ApplyInputBindings(InputManager& inputManager, const InputConfig& inputConfig, std::string_view managerName)
	{
		inputManager.ClearActionBindings();
		for (const auto& [actionName, keyCodes] : inputConfig.actions)
		{
			inputManager.SetActionBindings(actionName, keyCodes);
		}
		LogAppliedInputBindings(inputManager, managerName);
	}

} // namespace NeneEngine
