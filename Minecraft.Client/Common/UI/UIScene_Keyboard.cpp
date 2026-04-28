#include "stdafx.h"
#include "UI.h"
#include "UIScene_Keyboard.h"
#include <deque>
#include <vector>

namespace
{
	Win64InGameKeyboard::KeyboardCallback g_keyboardCallback = NULL;
	void *g_keyboardCallbackParam = NULL;
	wchar_t *g_keyboardOutputBuffer = NULL;
	unsigned int g_keyboardOutputBufferChars = 0;
	bool g_keyboardActive = false;
	std::wstring g_keyboardLastText;
	unsigned int g_keyboardMaxChars = 1;
	bool g_keyboardSubmitRequested = false;
	bool g_keyboardCancelRequested = false;
	bool g_keyboardTextChanged = false;
	int g_keyboardCursorLeftRequests = 0;
	int g_keyboardCursorRightRequests = 0;
	std::deque<wchar_t> g_keyboardChars;
	CRITICAL_SECTION g_keyboardCS;
	volatile LONG g_keyboardCSInitState = 0;

	void EnsureKeyboardLock()
	{
		if (g_keyboardCSInitState == 2)
		{
			return;
		}

		if (InterlockedCompareExchange(&g_keyboardCSInitState, 1, 0) == 0)
		{
			InitializeCriticalSection(&g_keyboardCS);
			InterlockedExchange(&g_keyboardCSInitState, 2);
			return;
		}

		while (g_keyboardCSInitState != 2)
		{
			SwitchToThread();
		}
	}
}

bool Win64InGameKeyboard::Request(const wchar_t *title,
	const wchar_t *initialText,
	unsigned int iPad,
	unsigned int maxChars,
	KeyboardCallback callback,
	void *callbackParam,
	int keyboardMode,
	wchar_t *outputBuffer,
	unsigned int outputBufferChars)
{
	(void)keyboardMode;
	EnsureKeyboardLock();

	if (g_keyboardActive)
	{
		Complete(false, L"");
	}

	UIScene_KeyboardInitData *initData = new UIScene_KeyboardInitData();
	initData->title = title != NULL ? title : L"";
	initData->initialText = initialText != NULL ? initialText : L"";
	initData->maxChars = (maxChars > 0) ? maxChars : 1;

	EnterCriticalSection(&g_keyboardCS);
	g_keyboardCallback = callback;
	g_keyboardCallbackParam = callbackParam;
	g_keyboardOutputBuffer = outputBuffer;
	g_keyboardOutputBufferChars = outputBufferChars;
	g_keyboardActive = true;
	g_keyboardLastText = initData->initialText;
	g_keyboardMaxChars = initData->maxChars;
	g_keyboardSubmitRequested = false;
	g_keyboardCancelRequested = false;
	g_keyboardTextChanged = false;
	g_keyboardCursorLeftRequests = 0;
	g_keyboardCursorRightRequests = 0;
	g_keyboardChars.clear();
	LeaveCriticalSection(&g_keyboardCS);

	ui.NavigateToScene((int)iPad, eUIScene_Keyboard, initData);
	return true;
}

void Win64InGameKeyboard::Complete(bool accepted, const wchar_t *text)
{
	EnsureKeyboardLock();

	KeyboardCallback callback = NULL;
	void *callbackParam = NULL;

	EnterCriticalSection(&g_keyboardCS);
	if (!g_keyboardActive)
	{
		LeaveCriticalSection(&g_keyboardCS);
		return;
	}

	if (accepted && text != NULL)
	{
		g_keyboardLastText = text;
	}
	else
	{
		g_keyboardLastText.clear();
	}

	if (g_keyboardOutputBuffer != NULL && g_keyboardOutputBufferChars > 0)
	{
		if (accepted)
		{
			wcsncpy_s(g_keyboardOutputBuffer, g_keyboardOutputBufferChars, g_keyboardLastText.c_str(), _TRUNCATE);
		}
		else
		{
			g_keyboardOutputBuffer[0] = 0;
		}
	}

	callback = g_keyboardCallback;
	callbackParam = g_keyboardCallbackParam;

	g_keyboardCallback = NULL;
	g_keyboardCallbackParam = NULL;
	g_keyboardOutputBuffer = NULL;
	g_keyboardOutputBufferChars = 0;
	g_keyboardActive = false;
	g_keyboardMaxChars = 1;
	g_keyboardSubmitRequested = false;
	g_keyboardCancelRequested = false;
	g_keyboardTextChanged = false;
	g_keyboardCursorLeftRequests = 0;
	g_keyboardCursorRightRequests = 0;
	g_keyboardChars.clear();
	LeaveCriticalSection(&g_keyboardCS);

	if (callback != NULL)
	{
		callback(callbackParam, accepted);
	}
}

void Win64InGameKeyboard::GetText(uint16_t *utf16Text)
{
	if (utf16Text == NULL)
	{
		return;
	}
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	wchar_t *dest = (wchar_t *)utf16Text;
	const wchar_t *src = g_keyboardLastText.c_str();
	while ((*dest++ = *src++) != 0)
	{
	}
	LeaveCriticalSection(&g_keyboardCS);
}

void Win64InGameKeyboard::OnChar(wchar_t ch)
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	if (!g_keyboardActive)
	{
		LeaveCriticalSection(&g_keyboardCS);
		return;
	}

	if (ch == L'\r' || ch == L'\n')
	{
		g_keyboardSubmitRequested = true;
		LeaveCriticalSection(&g_keyboardCS);
		return;
	}

	if (ch == L'\b')
	{
		if (!g_keyboardLastText.empty())
		{
			g_keyboardLastText.erase(g_keyboardLastText.length() - 1, 1);
			g_keyboardTextChanged = true;
		}
		LeaveCriticalSection(&g_keyboardCS);
		return;
	}

	if (ch >= 32 && g_keyboardLastText.length() < g_keyboardMaxChars)
	{
		g_keyboardLastText.push_back(ch);
		g_keyboardTextChanged = true;
	}

	LeaveCriticalSection(&g_keyboardCS);
}

void Win64InGameKeyboard::OnVirtualKeyDown(unsigned int vk)
{
	const unsigned int kVirtualKeyLeft = 0x25;
	const unsigned int kVirtualKeyRight = 0x27;
	const unsigned int kVirtualKeyEscape = 0x1B;

	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	if (!g_keyboardActive)
	{
		LeaveCriticalSection(&g_keyboardCS);
		return;
	}

	if (vk == kVirtualKeyLeft)
	{
		++g_keyboardCursorLeftRequests;
	}
	else if (vk == kVirtualKeyRight)
	{
		++g_keyboardCursorRightRequests;
	}
	else if (vk == kVirtualKeyEscape)
	{
		g_keyboardCancelRequested = true;
	}

	LeaveCriticalSection(&g_keyboardCS);
}

bool Win64InGameKeyboard::ConsumeSubmitRequested()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool submitRequested = g_keyboardSubmitRequested;
	g_keyboardSubmitRequested = false;
	LeaveCriticalSection(&g_keyboardCS);

	return submitRequested;
}

bool Win64InGameKeyboard::ConsumeTextChanged()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool textChanged = g_keyboardTextChanged;
	g_keyboardTextChanged = false;
	LeaveCriticalSection(&g_keyboardCS);

	return textChanged;
}

bool Win64InGameKeyboard::ConsumeCursorLeftRequested()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool requested = (g_keyboardCursorLeftRequests > 0);
	if (requested)
		--g_keyboardCursorLeftRequests;
	LeaveCriticalSection(&g_keyboardCS);

	return requested;
}

bool Win64InGameKeyboard::ConsumeCursorRightRequested()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool requested = (g_keyboardCursorRightRequests > 0);
	if (requested)
		--g_keyboardCursorRightRequests;
	LeaveCriticalSection(&g_keyboardCS);

	return requested;
}

bool Win64InGameKeyboard::ConsumeCancelRequested()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool requested = g_keyboardCancelRequested;
	g_keyboardCancelRequested = false;
	LeaveCriticalSection(&g_keyboardCS);

	return requested;
}

bool Win64InGameKeyboard::PopChar(wchar_t *outCh)
{
	if (outCh == NULL)
	{
		return false;
	}

	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	if (g_keyboardChars.empty())
	{
		LeaveCriticalSection(&g_keyboardCS);
		return false;
	}

	*outCh = g_keyboardChars.front();
	g_keyboardChars.pop_front();
	LeaveCriticalSection(&g_keyboardCS);
	return true;
}

bool Win64InGameKeyboard::IsActive()
{
	EnsureKeyboardLock();

	EnterCriticalSection(&g_keyboardCS);
	const bool isActive = g_keyboardActive;
	LeaveCriticalSection(&g_keyboardCS);
	return isActive;
}

#define KEYBOARD_DONE_TIMER_ID 0
#define KEYBOARD_DONE_TIMER_TIME 100
#define KEYBOARD_BACKGROUND_OPACITY 0.72f

UIScene_Keyboard::UIScene_Keyboard(int iPad, void *initData, UILayer *parentLayer) : UIScene(iPad, parentLayer)
{
	UIScene_KeyboardInitData *params = (UIScene_KeyboardInitData *)initData;
	wstring title = L"Enter Text";
	wstring initialText = L"";
	unsigned int maxChars = 15;
	if (params != NULL)
	{
		if (!params->title.empty())
			title = params->title;
		initialText = params->initialText;
		if (params->maxChars > 0)
			maxChars = params->maxChars;
		delete params;
	}

	// Setup all the Iggy references we need for this scene
	initialiseMovie();

	m_EnterTextLabel.init(title);

	m_KeyboardTextInput.init(initialText, -1);
	m_KeyboardTextInput.SetCharLimit((int)maxChars);
	m_charLimit = (int)maxChars;
	
	m_ButtonSpace.init(L"Space", -1);
	m_ButtonCursorLeft.init(L"Cursor Left", -1);
	m_ButtonCursorRight.init(L"Cursor Right", -1);
	m_ButtonCaps.init(L"Caps", -1);
	m_ButtonDone.init(L"Done", 0);	// only the done button needs an id, the others will never call back!
	m_ButtonSymbols.init(L"Symbols", -1);
	m_ButtonBackspace.init(L"Backspace", -1);

	// Initialise function keyboard Buttons and set alternative symbol button string
	wstring label = L"Abc";
	IggyStringUTF16 stringVal;
	stringVal.string = (IggyUTF16*)label.c_str();
	stringVal.length = label.length();
	
	IggyDataValue result;
	IggyDataValue value[1];
	value[0].type = IGGY_DATATYPE_string_UTF16;
	value[0].string16 = stringVal;

	IggyResult out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcInitFunctionButtons , 1 , value );

	m_bKeyboardDonePressed = false;
	m_bKeyboardResultSent = false;
	m_pMenuBackground = NULL;

	m_pMenuBackground = parentLayer->addComponent(iPad,eUIComponent_MenuBackground);
	if (m_pMenuBackground != NULL)
	{
		m_pMenuBackground->setOpacity(KEYBOARD_BACKGROUND_OPACITY);
	}
}

UIScene_Keyboard::~UIScene_Keyboard()
{
	if (!m_bKeyboardResultSent && Win64InGameKeyboard::IsActive())
	{
		Win64InGameKeyboard::Complete(false, L"");
		m_bKeyboardResultSent = true;
	}

	if (m_pMenuBackground != NULL)
	{
		m_pMenuBackground->setOpacity(1.0f);
		m_pMenuBackground = NULL;
	}

	m_parentLayer->removeComponent(eUIComponent_MenuBackground);
}

void UIScene_Keyboard::tick()
{
	UIScene::tick();

	if (!Win64InGameKeyboard::IsActive())
	{
		return;
	}

	if (Win64InGameKeyboard::ConsumeTextChanged())
	{
		std::vector<uint16_t> utf16Text((size_t)m_charLimit + 1u);
		ZeroMemory(&utf16Text[0], utf16Text.size() * sizeof(uint16_t));
		Win64InGameKeyboard::GetText(&utf16Text[0]);
		m_KeyboardTextInput.setLabel((wchar_t *)&utf16Text[0]);
	}

	IggyDataValue result;
	while (Win64InGameKeyboard::ConsumeCursorLeftRequested())
	{
		IggyPlayerCallMethodRS(getMovie(), &result, IggyPlayerRootPath(getMovie()), m_funcCursorLeftButtonPressed, 0, NULL);
	}
	while (Win64InGameKeyboard::ConsumeCursorRightRequested())
	{
		IggyPlayerCallMethodRS(getMovie(), &result, IggyPlayerRootPath(getMovie()), m_funcCursorRightButtonPressed, 0, NULL);
	}

	if (Win64InGameKeyboard::ConsumeCancelRequested())
	{
		if (!m_bKeyboardResultSent)
		{
			Win64InGameKeyboard::Complete(false, L"");
			m_bKeyboardResultSent = true;
		}
		navigateBack();
		return;
	}

	if (Win64InGameKeyboard::ConsumeSubmitRequested() && !m_bKeyboardDonePressed)
	{
		addTimer(KEYBOARD_DONE_TIMER_ID, KEYBOARD_DONE_TIMER_TIME);
		m_bKeyboardDonePressed = true;
	}
}

wstring UIScene_Keyboard::getMoviePath()
{
	if(app.GetLocalPlayerCount() > 1 && !m_parentLayer->IsFullscreenGroup())
	{
		return L"KeyboardSplit";
	}
	else
	{
		return L"Keyboard";
	}
}

void UIScene_Keyboard::updateTooltips()
{
	ui.SetTooltips( DEFAULT_XUI_MENU_USER, IDS_TOOLTIPS_SELECT,IDS_TOOLTIPS_BACK, -1, -1);
}

bool UIScene_Keyboard::allowRepeat(int key)
{
	// 4J - TomK - we want to allow X and Y repeats!
	switch(key)
	{
	case ACTION_MENU_OK:
	case ACTION_MENU_CANCEL:
	case ACTION_MENU_A:
	case ACTION_MENU_B:
	case ACTION_MENU_PAUSEMENU:
	//case ACTION_MENU_X:
	//case ACTION_MENU_Y:
		return false;
	}
	return true;
}

void UIScene_Keyboard::handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled)
{
	IggyDataValue result;
	IggyResult out;

	if(repeat || pressed)
	{
		switch(key)
		{
		case ACTION_MENU_CANCEL:
		case ACTION_MENU_B:
			if (!m_bKeyboardResultSent)
			{
				Win64InGameKeyboard::Complete(false, L"");
				m_bKeyboardResultSent = true;
			}
			navigateBack();
			handled = true;
			break;
		case ACTION_MENU_X:					// X
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcBackspaceButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_PAGEUP:			// LT
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcSymbolButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_Y:					// Y
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcSpaceButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_STICK_PRESS:		// LS
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcCapsButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_LEFT_SCROLL:		// LB
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcCursorLeftButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_RIGHT_SCROLL:		// RB
			out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcCursorRightButtonPressed, 0 , NULL );
			handled = true;
			break;
		case ACTION_MENU_PAUSEMENU:			// Start
			if(!m_bKeyboardDonePressed)
			{
				out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcDoneButtonPressed, 0 , NULL );
				
				// kick off done timer
				addTimer(KEYBOARD_DONE_TIMER_ID,KEYBOARD_DONE_TIMER_TIME);
				m_bKeyboardDonePressed = true;
			}
			handled = true;
			break;
		}
	}

	switch(key)
	{
	case ACTION_MENU_OK:
	case ACTION_MENU_LEFT:
	case ACTION_MENU_RIGHT:
	case ACTION_MENU_UP:
	case ACTION_MENU_DOWN:
		sendInputToMovie(key, repeat, pressed, released);
		handled = true;
		break;
	}
}

void UIScene_Keyboard::handlePress(F64 controlId, F64 childId)
{
	if((int)controlId == 0)
	{
		// Done has been pressed. At this point we can query for the input string and pass it on to wherever it is needed.
		// we can not query for m_KeyboardTextInput.getLabel() here because we're in an iggy callback so we need to wait a frame.
		if(!m_bKeyboardDonePressed)
		{
			// kick off done timer
			addTimer(KEYBOARD_DONE_TIMER_ID,KEYBOARD_DONE_TIMER_TIME);
			m_bKeyboardDonePressed = true;
		}
	}
}

void UIScene_Keyboard::handleTimerComplete(int id)
{
	if(id == KEYBOARD_DONE_TIMER_ID)
	{
		// remove timer
		killTimer(KEYBOARD_DONE_TIMER_ID);

		// we're done here!
		KeyboardDonePressed();
	}
}

void UIScene_Keyboard::KeyboardDonePressed()
{
	if (!m_bKeyboardResultSent)
	{
		std::vector<uint16_t> utf16Text((size_t)m_charLimit + 1u);
		ZeroMemory(&utf16Text[0], utf16Text.size() * sizeof(uint16_t));
		Win64InGameKeyboard::GetText(&utf16Text[0]);
		Win64InGameKeyboard::Complete(true, (wchar_t *)&utf16Text[0]);
		m_bKeyboardResultSent = true;
	}

	// Debug
	app.DebugPrintf("UI Keyboard - DONE - [%ls]\n", m_KeyboardTextInput.getLabel());

	// ToDo: Keyboard can now pass on its final string value and close itself down
	navigateBack();
}