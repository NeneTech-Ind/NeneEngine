// InputDevice.h

#pragma once

#include "Core/Delegate.h"
#include "Input/KeyCode.h"

#include <EASTL/unordered_set.h>
#include <glm/glm.hpp>

namespace NeneEngine
{

	struct KeyEvent
	{
		KeyCode key = KeyCode::None;
		bool wasDown = false;
	};

	struct MouseMoveEvent
	{
		glm::vec2 position = {0.0f, 0.0f};
		glm::vec2 delta = {0.0f, 0.0f};
	};

	struct MouseWheelEvent
	{
		glm::vec2 position = {0.0f, 0.0f};
		float delta = 0.0f;
	};

	class InputDevice
	{
	  public:
		MulticastDelegate<const KeyEvent&> KeyDown;
		MulticastDelegate<const KeyEvent&> KeyUp;
		MulticastDelegate<const MouseMoveEvent&> MouseMoved;
		MulticastDelegate<const MouseWheelEvent&> MouseWheel;

		void NotifyKeyDown(KeyCode key);
		void NotifyKeyUp(KeyCode key);
		void NotifyMouseMove(glm::vec2 position);
		void NotifyMouseDelta(glm::vec2 delta, glm::vec2 position);
		void NotifyMouseWheel(float delta);
		void EndFrame();
		void SetFocused(bool focused);
		void ResetState();

		bool IsKeyDown(KeyCode key) const;
		bool IsKeyPressed(KeyCode key) const;
		bool IsFocused() const { return m_isFocused; }
		glm::vec2 GetMousePosition() const { return m_mousePosition; }
		glm::vec2 GetMouseDelta() const { return m_mouseDelta; }
		float GetMouseWheelDelta() const { return m_mouseWheelDelta; }

	  private:
		eastl::unordered_set<KeyCode> m_pressedKeys;
		eastl::unordered_set<KeyCode> m_pressedThisFrame;
		glm::vec2 m_mousePosition = {0.0f, 0.0f};
		glm::vec2 m_mouseDelta = {0.0f, 0.0f};
		float m_mouseWheelDelta = 0.0f;
		bool m_isFocused = true;
	};

} // namespace NeneEngine
