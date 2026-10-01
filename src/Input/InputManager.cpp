// InputManager.cpp

#include "Input/InputManager.h"
#include "Input/InputDevice.h"

#include <algorithm>

namespace NeneEngine
{
	namespace
	{
		constexpr int kMouseButtonCount = 5;
	}

	InputManager::InputManager(const InputDevice* inputDevice) : m_inputDevice(inputDevice) {}

	void InputManager::SetInputDevice(const InputDevice* inputDevice)
	{
		if (m_inputDevice == inputDevice) return;

		m_inputDevice = inputDevice;
		ResetState();
	}

	void InputManager::UpdateState()
	{
		ResetState();
		if (m_inputDevice == nullptr) return;

		for (uint16_t value = static_cast<uint16_t>(KeyCode::Backspace);
		     value <= static_cast<uint16_t>(KeyCode::RightAlt); ++value)
		{
			const KeyCode key = static_cast<KeyCode>(value);
			if (m_inputDevice->IsKeyDown(key)) m_keyDownStates[key] = true;
			if (m_inputDevice->IsKeyPressed(key)) m_keyPressedStates[key] = true;
		}

		for (int buttonIndex = 0; buttonIndex < kMouseButtonCount; ++buttonIndex)
		{
			const KeyCode mouseKey = MouseButtonIndexToKeyCode(buttonIndex);
			const bool isDown = m_inputDevice->IsKeyDown(mouseKey);
			const bool isPressed = m_inputDevice->IsKeyPressed(mouseKey);

			m_mouseButtonDownStates[buttonIndex] = isDown;
			m_mouseButtonPressedStates[buttonIndex] = isPressed;
			m_keyDownStates[mouseKey] = isDown;
			m_keyPressedStates[mouseKey] = isPressed;
		}

		m_mousePosition = m_inputDevice->GetMousePosition();
		m_mouseDelta = m_inputDevice->GetMouseDelta();
	}

	bool InputManager::IsKeyDown(KeyCode key) const
	{
		const auto it = m_keyDownStates.find(key);
		return it != m_keyDownStates.end() && it->second;
	}

	bool InputManager::IsKeyPressed(KeyCode key) const
	{
		const auto it = m_keyPressedStates.find(key);
		return it != m_keyPressedStates.end() && it->second;
	}

	bool InputManager::IsMouseButtonDown(int buttonIndex) const
	{
		const auto it = m_mouseButtonDownStates.find(buttonIndex);
		return it != m_mouseButtonDownStates.end() && it->second;
	}

	bool InputManager::IsMouseButtonPressed(int buttonIndex) const
	{
		const auto it = m_mouseButtonPressedStates.find(buttonIndex);
		return it != m_mouseButtonPressedStates.end() && it->second;
	}

	void InputManager::BindAction(const std::string& actionName, KeyCode keyCode)
	{
		auto& bindings = m_actionsMap[actionName];
		if (std::find(bindings.begin(), bindings.end(), keyCode) == bindings.end()) bindings.push_back(keyCode);
	}

	void InputManager::SetActionBindings(const std::string& actionName, eastl::vector<KeyCode> keyCodes)
	{
		if (keyCodes.empty())
		{
			m_actionsMap.erase(actionName);
			return;
		}

		auto& bindings = m_actionsMap[actionName];
		bindings.clear();
		for (KeyCode keyCode : keyCodes)
		{
			if (std::find(bindings.begin(), bindings.end(), keyCode) == bindings.end()) bindings.push_back(keyCode);
		}
	}

	void InputManager::ClearActionBindings()
	{
		m_actionsMap.clear();
	}

	bool InputManager::IsActionActive(std::string_view actionName) const
	{
		const auto it = m_actionsMap.find(std::string(actionName));
		if (it == m_actionsMap.end()) return false;

		for (KeyCode keyCode : it->second)
		{
			if (IsKeyDown(keyCode)) return true;
		}

		return false;
	}

	bool InputManager::IsActionPressed(std::string_view actionName) const
	{
		const auto it = m_actionsMap.find(std::string(actionName));
		if (it == m_actionsMap.end()) return false;

		for (KeyCode keyCode : it->second)
		{
			if (IsKeyPressed(keyCode)) return true;
		}

		return false;
	}

	void InputManager::ResetState()
	{
		m_keyDownStates.clear();
		m_keyPressedStates.clear();
		m_mouseButtonDownStates.clear();
		m_mouseButtonPressedStates.clear();
		m_mousePosition = {0.0f, 0.0f};
		m_mouseDelta = {0.0f, 0.0f};
	}

	KeyCode InputManager::MouseButtonIndexToKeyCode(int buttonIndex)
	{
		switch (buttonIndex)
		{
		case 0:
			return KeyCode::MouseLeft;
		case 1:
			return KeyCode::MouseRight;
		case 2:
			return KeyCode::MouseMiddle;
		case 3:
			return KeyCode::MouseX1;
		case 4:
			return KeyCode::MouseX2;
		default:
			return KeyCode::None;
		}
	}

} // namespace NeneEngine
