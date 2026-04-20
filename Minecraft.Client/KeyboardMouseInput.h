#pragma once

#include <windows.h>

class KeyboardMouseInput
{
public:
	static const int MAX_KEYS = 256;

	static const int MOUSE_LEFT = 0;
	static const int MOUSE_RIGHT = 1;
	static const int MOUSE_MIDDLE = 2;
	static const int MAX_MOUSE_BUTTONS = 3;

	static int KEY_FORWARD;
	static int KEY_BACKWARD;
	static int KEY_LEFT;
	static int KEY_RIGHT;
	static int KEY_JUMP;
	static int KEY_SNEAK;
	static int KEY_SPRINT;
	static int KEY_INVENTORY;
	static int KEY_DROP;
	static int KEY_CHAT;
	static int KEY_CRAFTING;
	static const int KEY_CRAFTING_ALT = 'R';
	static int KEY_CONFIRM;
	static const int KEY_CANCEL = VK_BACK;
	static int KEY_PAUSE;
	static int KEY_THIRD_PERSON;
	static int KEY_DEBUG_INFO;
	static int KEY_VOICE;

	void Init();
	void Tick();
	void ClearAllState();

	void OnKeyDown(int vkCode);
	void OnKeyUp(int vkCode);
	void OnMouseButtonDown(int button);
	void OnMouseButtonUp(int button);
	void OnMouseMove(int x, int y);
	void OnMouseWheel(int delta);
	void OnRawMouseDelta(int dx, int dy);

	bool IsKeyDown(int vkCode) const;
	bool IsKeyPressed(int vkCode) const;
	bool IsKeyReleased(int vkCode) const;

	bool IsMouseButtonDown(int button) const;
	bool IsMouseButtonPressed(int button) const;
	bool IsMouseButtonReleased(int button) const;

	int GetMouseX() const { return m_mouseX; }
	int GetMouseY() const { return m_mouseY; }

	int GetMouseDeltaX() const { return m_mouseDeltaX; }
	int GetMouseDeltaY() const { return m_mouseDeltaY; }

	bool HadRawMouseInput() const { return m_hadRawMouseInput; }

	int GetMouseWheel() const { return m_mouseWheel; }
	int ConsumeMouseWheel() { int wheel = m_mouseWheel; m_mouseWheel = 0; return wheel; }

	void SetMouseGrabbed(bool grabbed);
	bool IsMouseGrabbed() const { return m_mouseGrabbed; }

	void SetCursorHiddenForUI(bool hidden);
	bool IsCursorHiddenForUI() const { return m_cursorHiddenForUI; }

	void SetWindowFocused(bool focused);
	bool IsWindowFocused() const { return m_windowFocused; }

	bool HasAnyInput() const { return m_hasInput; }

	void SetKBMActive(bool active) { m_kbmActive = active; }
	bool IsKBMActive() const { return m_kbmActive; }

	void SetScreenCursorHidden(bool hidden) { m_screenWantsCursorHidden = hidden; }
	bool IsScreenCursorHidden() const { return m_screenWantsCursorHidden; }

	float GetMoveX() const;
	float GetMoveY() const;

	float GetLookX(float sensitivity) const;
	float GetLookY(float sensitivity) const;

	int GetRawDeltaX() const { return m_mouseDeltaAccumX; }
	int GetRawDeltaY() const { return m_mouseDeltaAccumY; }
	void ConsumeMouseDelta() { m_mouseDeltaAccumX = 0; m_mouseDeltaAccumY = 0; }

private:
	bool m_keyDown[MAX_KEYS];
	bool m_keyDownPrev[MAX_KEYS];

	bool m_keyPressedAccum[MAX_KEYS];
	bool m_keyReleasedAccum[MAX_KEYS];
	bool m_keyPressed[MAX_KEYS];
	bool m_keyReleased[MAX_KEYS];

	bool m_mouseButtonDown[MAX_MOUSE_BUTTONS];
	bool m_mouseButtonDownPrev[MAX_MOUSE_BUTTONS];

	bool m_mouseBtnPressedAccum[MAX_MOUSE_BUTTONS];
	bool m_mouseBtnReleasedAccum[MAX_MOUSE_BUTTONS];
	bool m_mouseBtnPressed[MAX_MOUSE_BUTTONS];
	bool m_mouseBtnReleased[MAX_MOUSE_BUTTONS];

	int m_mouseX;
	int m_mouseY;

	int m_mouseDeltaX;
	int m_mouseDeltaY;
	int m_mouseDeltaAccumX;
	int m_mouseDeltaAccumY;

	int m_mouseWheel;
	int m_mouseWheelAccum;
	int m_mouseWheelRemainder;

	bool m_mouseGrabbed;

	bool m_cursorHiddenForUI;

	bool m_windowFocused;

	bool m_hasInput;

	bool m_kbmActive;

	bool m_screenWantsCursorHidden;

	bool m_hadRawMouseInput;
};

extern KeyboardMouseInput g_KBMInput;
