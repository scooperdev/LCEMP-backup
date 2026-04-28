#include "stdafx.h"
#include "KeyboardMouseInput.h"
#if defined(_WINDOWS64) && !defined(_DEDICATED_SERVER)
#include "Windows64/KBMConfig.h"
#endif
#include <cmath>

KeyboardMouseInput g_KBMInput;

int KeyboardMouseInput::KEY_FORWARD = 'W';
int KeyboardMouseInput::KEY_BACKWARD = 'S';
int KeyboardMouseInput::KEY_LEFT = 'A';
int KeyboardMouseInput::KEY_RIGHT = 'D';
int KeyboardMouseInput::KEY_JUMP = VK_SPACE;
int KeyboardMouseInput::KEY_SNEAK = VK_LSHIFT;
int KeyboardMouseInput::KEY_SPRINT = VK_LCONTROL;
int KeyboardMouseInput::KEY_INVENTORY = 'E';
int KeyboardMouseInput::KEY_DROP = 'Q';
int KeyboardMouseInput::KEY_CHAT = 'T';
int KeyboardMouseInput::KEY_CRAFTING = VK_TAB;
int KeyboardMouseInput::KEY_CONFIRM = VK_RETURN;
int KeyboardMouseInput::KEY_PAUSE = VK_ESCAPE;
int KeyboardMouseInput::KEY_THIRD_PERSON = VK_F5;
int KeyboardMouseInput::KEY_DEBUG_INFO = VK_F3;
int KeyboardMouseInput::KEY_VOICE = 'V';

extern HWND g_hWnd;

// Forward declaration
static void ClipCursorToWindow(HWND hWnd);

// coded by notpies fr
void KeyboardMouseInput::Init()
{
	#if defined(_WINDOWS64) && !defined(_DEDICATED_SERVER)
	KBMConfig& cfg = KBMConfig::Get();
	KeyboardMouseInput::KEY_FORWARD = cfg.keyForward;
	KeyboardMouseInput::KEY_BACKWARD = cfg.keyBackward;
	KeyboardMouseInput::KEY_LEFT = cfg.keyLeft;
	KeyboardMouseInput::KEY_RIGHT = cfg.keyRight;
	KeyboardMouseInput::KEY_JUMP = cfg.keyJump;
	KeyboardMouseInput::KEY_SNEAK = cfg.keySneak;
	KeyboardMouseInput::KEY_SPRINT = cfg.keySprint;
	KeyboardMouseInput::KEY_INVENTORY = cfg.keyInventory;
	KeyboardMouseInput::KEY_DROP = cfg.keyDrop;
	KeyboardMouseInput::KEY_CHAT = cfg.keyChat;
	KeyboardMouseInput::KEY_CRAFTING = cfg.keyCrafting;
	KeyboardMouseInput::KEY_CONFIRM = cfg.keyConfirm;
	KeyboardMouseInput::KEY_PAUSE = cfg.keyPause;
	KeyboardMouseInput::KEY_THIRD_PERSON = cfg.keyThirdPerson;
	KeyboardMouseInput::KEY_DEBUG_INFO = cfg.keyDebugInfo;
	KeyboardMouseInput::KEY_VOICE = cfg.keyVoice;
	#endif

	memset(m_keyDown, 0, sizeof(m_keyDown));
	memset(m_keyDownPrev, 0, sizeof(m_keyDownPrev));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));
	memset(m_keyPressed, 0, sizeof(m_keyPressed));
	memset(m_keyReleased, 0, sizeof(m_keyReleased));
	memset(m_mouseButtonDown, 0, sizeof(m_mouseButtonDown));
	memset(m_mouseButtonDownPrev, 0, sizeof(m_mouseButtonDownPrev));
	memset(m_mouseBtnPressedAccum, 0, sizeof(m_mouseBtnPressedAccum));
	memset(m_mouseBtnReleasedAccum, 0, sizeof(m_mouseBtnReleasedAccum));
	memset(m_mouseBtnPressed, 0, sizeof(m_mouseBtnPressed));
	memset(m_mouseBtnReleased, 0, sizeof(m_mouseBtnReleased));
	m_mouseX = 0;
	m_mouseY = 0;
	m_mouseDeltaX = 0;
	m_mouseDeltaY = 0;
	m_mouseDeltaAccumX = 0;
	m_mouseDeltaAccumY = 0;
	m_mouseWheel = 0;
	m_mouseWheelAccum = 0;
	m_mouseWheelRemainder = 0;
	m_mouseGrabbed = false;
	m_cursorHiddenForUI = false;
	m_windowFocused = true;
	m_hasInput = false;
	m_kbmActive = true;
	m_screenWantsCursorHidden = false;
	m_hadRawMouseInput = false;

	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
	rid.dwFlags = 0;
	rid.hwndTarget = g_hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

}

void KeyboardMouseInput::ClearAllState()
{
	memset(m_keyDown, 0, sizeof(m_keyDown));
	memset(m_keyDownPrev, 0, sizeof(m_keyDownPrev));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));
	memset(m_keyPressed, 0, sizeof(m_keyPressed));
	memset(m_keyReleased, 0, sizeof(m_keyReleased));
	memset(m_mouseButtonDown, 0, sizeof(m_mouseButtonDown));
	memset(m_mouseButtonDownPrev, 0, sizeof(m_mouseButtonDownPrev));
	memset(m_mouseBtnPressedAccum, 0, sizeof(m_mouseBtnPressedAccum));
	memset(m_mouseBtnReleasedAccum, 0, sizeof(m_mouseBtnReleasedAccum));
	memset(m_mouseBtnPressed, 0, sizeof(m_mouseBtnPressed));
	memset(m_mouseBtnReleased, 0, sizeof(m_mouseBtnReleased));
	m_mouseDeltaX = 0;
	m_mouseDeltaY = 0;
	m_mouseDeltaAccumX = 0;
	m_mouseDeltaAccumY = 0;
	m_mouseWheel = 0;
	m_mouseWheelAccum = 0;
	m_mouseWheelRemainder = 0;
	m_hadRawMouseInput = false;
}

void KeyboardMouseInput::Tick()
{
	memcpy(m_keyDownPrev, m_keyDown, sizeof(m_keyDown));
	memcpy(m_mouseButtonDownPrev, m_mouseButtonDown, sizeof(m_mouseButtonDown));

	memcpy(m_keyPressed, m_keyPressedAccum, sizeof(m_keyPressedAccum));
	memcpy(m_keyReleased, m_keyReleasedAccum, sizeof(m_keyReleasedAccum));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));

	memcpy(m_mouseBtnPressed, m_mouseBtnPressedAccum, sizeof(m_mouseBtnPressedAccum));
	memcpy(m_mouseBtnReleased, m_mouseBtnReleasedAccum, sizeof(m_mouseBtnReleasedAccum));
	memset(m_mouseBtnPressedAccum, 0, sizeof(m_mouseBtnPressedAccum));
	memset(m_mouseBtnReleasedAccum, 0, sizeof(m_mouseBtnReleasedAccum));

	m_mouseDeltaX = m_mouseDeltaAccumX;
	m_mouseDeltaY = m_mouseDeltaAccumY;
	m_mouseDeltaAccumX = 0;
	m_mouseDeltaAccumY = 0;


	int wheelTotal = m_mouseWheelRemainder + m_mouseWheelAccum;
	int wheelSteps = wheelTotal / WHEEL_DELTA;
	m_mouseWheelRemainder = wheelTotal - (wheelSteps * WHEEL_DELTA);
	m_mouseWheelAccum = 0;
	m_mouseWheel += wheelSteps;

	m_hasInput = (m_mouseDeltaX != 0 || m_mouseDeltaY != 0 || wheelSteps != 0 || m_hadRawMouseInput);
	m_hadRawMouseInput = false;
	if (!m_hasInput)
	{
		for (int i = 0; i < MAX_KEYS; i++)
		{
			if (m_keyDown[i]) { m_hasInput = true; break; }
		}
	}
	if (!m_hasInput)
	{
		for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
		{
			if (m_mouseButtonDown[i]) { m_hasInput = true; break; }
		}
	}

	if ((m_mouseGrabbed || m_cursorHiddenForUI) && g_hWnd)
	{
		RECT rc;
		GetClientRect(g_hWnd, &rc);
		POINT center;
		center.x = (rc.right - rc.left) / 2;
		center.y = (rc.bottom - rc.top) / 2;
		ClientToScreen(g_hWnd, &center);
		SetCursorPos(center.x, center.y);
	}
}

void KeyboardMouseInput::OnKeyDown(int vkCode)
{
	if (vkCode >= 0 && vkCode < MAX_KEYS)
	{
		if (!m_keyDown[vkCode])
			m_keyPressedAccum[vkCode] = true;
		m_keyDown[vkCode] = true;
	}
}

void KeyboardMouseInput::OnKeyUp(int vkCode)
{
	if (vkCode >= 0 && vkCode < MAX_KEYS)
	{
		if (m_keyDown[vkCode])
			m_keyReleasedAccum[vkCode] = true;
		m_keyDown[vkCode] = false;
	}
}

void KeyboardMouseInput::OnMouseButtonDown(int button)
{
	if (button >= 0 && button < MAX_MOUSE_BUTTONS)
	{
		if (!m_mouseButtonDown[button])
			m_mouseBtnPressedAccum[button] = true;
		m_mouseButtonDown[button] = true;
	}
}

void KeyboardMouseInput::OnMouseButtonUp(int button)
{
	if (button >= 0 && button < MAX_MOUSE_BUTTONS)
	{
		if (m_mouseButtonDown[button])
			m_mouseBtnReleasedAccum[button] = true;
		m_mouseButtonDown[button] = false;
	}
}

void KeyboardMouseInput::OnMouseMove(int x, int y)
{
	m_mouseX = x;
	m_mouseY = y;
}

void KeyboardMouseInput::OnMouseWheel(int delta)
{
	m_mouseWheelAccum += delta;
}

void KeyboardMouseInput::OnRawMouseDelta(int dx, int dy)
{
	m_mouseDeltaAccumX += dx;
	m_mouseDeltaAccumY += dy;
	m_hadRawMouseInput = true;
}

bool KeyboardMouseInput::IsKeyDown(int vkCode) const
{
	if (vkCode >= 0 && vkCode < MAX_KEYS)
		return m_keyDown[vkCode];
	return false;
}

bool KeyboardMouseInput::IsKeyPressed(int vkCode) const
{
	if (vkCode >= 0 && vkCode < MAX_KEYS)
		return m_keyPressed[vkCode];
	return false;
}

bool KeyboardMouseInput::IsKeyReleased(int vkCode) const
{
	if (vkCode >= 0 && vkCode < MAX_KEYS)
		return m_keyReleased[vkCode];
	return false;
}

bool KeyboardMouseInput::IsMouseButtonDown(int button) const
{
	if (button >= 0 && button < MAX_MOUSE_BUTTONS)
		return m_mouseButtonDown[button];
	return false;
}

bool KeyboardMouseInput::IsMouseButtonPressed(int button) const
{
	if (button >= 0 && button < MAX_MOUSE_BUTTONS)
		return m_mouseBtnPressed[button];
	return false;
}

bool KeyboardMouseInput::IsMouseButtonReleased(int button) const
{
	if (button >= 0 && button < MAX_MOUSE_BUTTONS)
		return m_mouseBtnReleased[button];
	return false;
}

void KeyboardMouseInput::SetMouseGrabbed(bool grabbed)
{
	if (m_mouseGrabbed == grabbed)
		return;

	m_mouseGrabbed = grabbed;
	if (grabbed && g_hWnd)
	{
		while (ShowCursor(FALSE) >= 0) {}
		ClipCursorToWindow(g_hWnd);
		
		RECT rc;
		GetClientRect(g_hWnd, &rc);
		POINT center;
		center.x = (rc.right - rc.left) / 2;
		center.y = (rc.bottom - rc.top) / 2;
		ClientToScreen(g_hWnd, &center);
		SetCursorPos(center.x, center.y);

		m_mouseDeltaAccumX = 0;
		m_mouseDeltaAccumY = 0;
	}
	else if (!grabbed && !m_cursorHiddenForUI && g_hWnd)
	{
		while (ShowCursor(TRUE) < 0) {}
		ClipCursor(NULL);
	}

	m_mouseWheel = 0;
	m_mouseWheelAccum = 0;
	m_mouseWheelRemainder = 0;
}

void KeyboardMouseInput::SetCursorHiddenForUI(bool hidden)
{
	if (m_cursorHiddenForUI == hidden)
		return;

	m_cursorHiddenForUI = hidden;
	if (hidden && g_hWnd)
	{
		while (ShowCursor(FALSE) >= 0) {}
		ClipCursorToWindow(g_hWnd);

		RECT rc;
		GetClientRect(g_hWnd, &rc);
		POINT center;
		center.x = (rc.right - rc.left) / 2;
		center.y = (rc.bottom - rc.top) / 2;
		ClientToScreen(g_hWnd, &center);
		SetCursorPos(center.x, center.y);

		m_mouseDeltaAccumX = 0;
		m_mouseDeltaAccumY = 0;
	}
	else if (!hidden && !m_mouseGrabbed && g_hWnd)
	{
		while (ShowCursor(TRUE) < 0) {}
		ClipCursor(NULL);
	}
}

static void ClipCursorToWindow(HWND hWnd)
{
	if (!hWnd) return;
	RECT rc;
	GetClientRect(hWnd, &rc);
	POINT topLeft = { rc.left, rc.top };
	POINT bottomRight = { rc.right, rc.bottom };
	ClientToScreen(hWnd, &topLeft);
	ClientToScreen(hWnd, &bottomRight);
	RECT clipRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
	ClipCursor(&clipRect);
}

void KeyboardMouseInput::SetWindowFocused(bool focused)
{
	m_windowFocused = focused;
	if (focused)
	{
		if (m_mouseGrabbed || m_cursorHiddenForUI)
		{
			while (ShowCursor(FALSE) >= 0) {}
			ClipCursorToWindow(g_hWnd);
		}
		else
		{
			while (ShowCursor(TRUE) < 0) {}
			ClipCursor(NULL);
		}
	}
	else
	{
		while (ShowCursor(TRUE) < 0) {}
		ClipCursor(NULL);
	}
}

float KeyboardMouseInput::GetMoveX() const
{
	float x = 0.0f;
	if (m_keyDown[KEY_LEFT])  x += 1.0f;
	if (m_keyDown[KEY_RIGHT]) x -= 1.0f;
	return x;
}

float KeyboardMouseInput::GetMoveY() const
{
	float y = 0.0f;
	if (m_keyDown[KEY_FORWARD])  y += 1.0f;
	if (m_keyDown[KEY_BACKWARD]) y -= 1.0f;
	return y;
}

float KeyboardMouseInput::GetLookX(float sensitivity) const
{
	return (float)m_mouseDeltaX * sensitivity;
}

float KeyboardMouseInput::GetLookY(float sensitivity) const
{
	return (float)(-m_mouseDeltaY) * sensitivity;
}
