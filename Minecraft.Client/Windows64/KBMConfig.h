#pragma once

#ifdef _WINDOWS64

#include <windows.h>

struct KBMConfig
{
	int keyForward;
	int keyBackward;
	int keyLeft;
	int keyRight;
	int keyJump;
	int keySneak;
	int keySprint;
	int keyInventory;
	int keyDrop;
	int keyChat;
	int keyCrafting;
	int keyConfirm;
	int keyPause;
	int keyThirdPerson;
	int keyDebugInfo;
	int keyVoice;

	int voiceMode;

	static KBMConfig& Get();
	void Load();
};

#endif
