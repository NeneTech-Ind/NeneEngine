// InputDevice.cpp

#include "Input/InputDevice.h"

namespace NeneEngine
{

	void InputDevice::NotifyKeyDown(KeyCode key)
	{
		const bool wasDown = IsKeyDown(key);
		m_pressedKeys.insert(key);
		// IsKeyPressed is edge-triggered; held keys remain only in m_pressedKeys.
		if (!wasDown) m_pressedThisFrame.insert(key);

		m_keyDown.Broadcast(KeyEvent{key, wasDown});
	}

	void InputDevice::NotifyKeyUp(KeyCode key)
	{
		const bool wasDown = IsKeyDown(key);
		m_pressedKeys.erase(key);

		m_keyUp.Broadcast(KeyEvent{key, wasDown});
	}

	void InputDevice::NotifyMouseMove(glm::vec2 position)
	{
		const glm::vec2 delta = position - m_mousePosition;
		NotifyMouseDelta(delta, position);
	}

	void InputDevice::NotifyMouseDelta(glm::vec2 delta, glm::vec2 position)
	{
		m_mouseDelta += delta;
		m_mousePosition = position;

		m_mouseMoved.Broadcast(MouseMoveEvent{m_mousePosition, delta});
	}

	void InputDevice::NotifyMouseWheel(float delta)
	{
		m_mouseWheelDelta += delta;

		m_mouseWheel.Broadcast(MouseWheelEvent{m_mousePosition, delta});
	}

	void InputDevice::EndFrame()
	{
		// Transient input state is valid for exactly one application frame.
		m_pressedThisFrame.clear();
		m_mouseDelta = {0.0f, 0.0f};
		m_mouseWheelDelta = 0.0f;
	}

	void InputDevice::SetFocused(bool focused)
	{
		if (m_isFocused == focused) return;

		m_isFocused = focused;
		if (!m_isFocused) ResetState();
	}

	void InputDevice::ResetState()
	{
		m_pressedKeys.clear();
		m_pressedThisFrame.clear();
		m_mouseDelta = {0.0f, 0.0f};
		m_mouseWheelDelta = 0.0f;
	}

	bool InputDevice::IsKeyDown(KeyCode key) const
	{
		return m_pressedKeys.contains(key);
	}

	bool InputDevice::IsKeyPressed(KeyCode key) const
	{
		return m_pressedThisFrame.contains(key);
	}

} // namespace NeneEngine
