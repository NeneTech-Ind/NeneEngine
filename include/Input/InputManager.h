// InputManager.h

#pragma once

#include "Core/EASTLStdHash.h"
#include "Input/IInputHandler.h"

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <string>
#include <string_view>

namespace NeneEngine
{
	class InputDevice;

	class InputManager final : public IInputHandler
	{
	  public:
		InputManager() = default;
		explicit InputManager(const InputDevice* inputDevice);

		void SetInputDevice(const InputDevice* inputDevice);
		const InputDevice* GetInputDevice() const { return m_inputDevice; }
		bool HasInputDevice() const { return m_inputDevice != nullptr; }

		void UpdateState();

		bool IsKeyDown(KeyCode key) const override;
		bool IsKeyPressed(KeyCode key) const override;
		bool IsMouseButtonDown(int buttonIndex) const override;
		bool IsMouseButtonPressed(int buttonIndex) const override;
		glm::vec2 GetMousePosition() const override { return m_mousePosition; }
		glm::vec2 GetMouseDelta() const override { return m_mouseDelta; }

		void BindAction(const std::string& actionName, KeyCode keyCode);
		void SetActionBindings(const std::string& actionName, eastl::vector<KeyCode> keyCodes);
		void ClearActionBindings();
		bool IsActionActive(std::string_view actionName) const override;
		bool IsActionPressed(std::string_view actionName) const override;
		const eastl::unordered_map<std::string, eastl::vector<KeyCode>>& GetActionBindings() const
		{
			return m_actionsMap;
		}

	  private:
		void ResetState();
		static KeyCode MouseButtonIndexToKeyCode(int buttonIndex);

		const InputDevice* m_inputDevice = nullptr;
		eastl::unordered_map<KeyCode, bool> m_keyDownStates;
		eastl::unordered_map<KeyCode, bool> m_keyPressedStates;
		eastl::unordered_map<int, bool> m_mouseButtonDownStates;
		eastl::unordered_map<int, bool> m_mouseButtonPressedStates;
		glm::vec2 m_mousePosition = {0.0f, 0.0f};
		glm::vec2 m_mouseDelta = {0.0f, 0.0f};
		eastl::unordered_map<std::string, eastl::vector<KeyCode>> m_actionsMap;
	};

} // namespace NeneEngine
