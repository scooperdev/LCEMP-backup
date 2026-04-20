#include "stdafx.h"

#ifdef _WINDOWS64

#include "KBMConfig.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#if defined(__linux__)
#include <unistd.h>
#endif

static KBMConfig s_config;
static bool s_loaded = false;
static void WriteDefaultINI(const char* path);

static bool BuildAppDataINIPath(char* outPath, size_t outSize)
{
	if (!outPath || outSize == 0) return false;

	char dirPath[MAX_PATH];
	int written = 0;
#if defined(__linux__)
	const char* baseDir = getenv("XDG_DATA_HOME");
	if (baseDir != NULL && baseDir[0] != 0)
	{
		written = snprintf(dirPath, MAX_PATH, "%s/minecraft", baseDir);
	}
	else
	{
		baseDir = getenv("HOME");
		if (baseDir == NULL || baseDir[0] == 0) return false;
		written = snprintf(dirPath, MAX_PATH, "%s/.minecraft", baseDir);
	}
#else
	const char* baseDir = getenv("APPDATA");
	if (baseDir == NULL || baseDir[0] == 0) return false;
	written = _snprintf_s(dirPath, MAX_PATH, _TRUNCATE, "%s\\Minecraft", baseDir);
#endif
	if (written <= 0 || written >= MAX_PATH) return false;

	CreateDirectoryA(dirPath, NULL);

	#if defined(__linux__)
	written = snprintf(outPath, outSize, "%s/kbm_config.ini", dirPath);
	#else
	written = _snprintf_s(outPath, outSize, _TRUNCATE, "%s\\kbm_config.ini", dirPath);
	#endif
	if (written <= 0 || (size_t)written >= outSize) return false;
	return true;
}

static FILE* OpenINIWithCreate(const char* path)
{
	if (!path || !path[0]) return NULL;

	FILE* f = fopen(path, "r");
	if (!f)
	{
		WriteDefaultINI(path);
		f = fopen(path, "r");
	}

	return f;
}

static int ParseVK(const char* val)
{
	if (!val || !val[0]) return 0;

	while (*val == ' ' || *val == '\t') val++;

	if (_stricmp(val, "SPACE") == 0) return VK_SPACE;
	if (_stricmp(val, "LSHIFT") == 0) return VK_LSHIFT;
	if (_stricmp(val, "RSHIFT") == 0) return VK_RSHIFT;
	if (_stricmp(val, "LCONTROL") == 0 || _stricmp(val, "LCTRL") == 0) return VK_LCONTROL;
	if (_stricmp(val, "RCONTROL") == 0 || _stricmp(val, "RCTRL") == 0) return VK_RCONTROL;
	if (_stricmp(val, "TAB") == 0) return VK_TAB;
	if (_stricmp(val, "RETURN") == 0 || _stricmp(val, "ENTER") == 0) return VK_RETURN;
	if (_stricmp(val, "BACK") == 0 || _stricmp(val, "BACKSPACE") == 0) return VK_BACK;
	if (_stricmp(val, "ESCAPE") == 0 || _stricmp(val, "ESC") == 0) return VK_ESCAPE;
	if (_stricmp(val, "F1") == 0) return VK_F1;
	if (_stricmp(val, "F2") == 0) return VK_F2;
	if (_stricmp(val, "F3") == 0) return VK_F3;
	if (_stricmp(val, "F4") == 0) return VK_F4;
	if (_stricmp(val, "F5") == 0) return VK_F5;
	if (_stricmp(val, "F6") == 0) return VK_F6;
	if (_stricmp(val, "F7") == 0) return VK_F7;
	if (_stricmp(val, "F8") == 0) return VK_F8;
	if (_stricmp(val, "F9") == 0) return VK_F9;
	if (_stricmp(val, "F10") == 0) return VK_F10;
	if (_stricmp(val, "F11") == 0) return VK_F11;
	if (_stricmp(val, "F12") == 0) return VK_F12;
	if (_stricmp(val, "LALT") == 0) return VK_LMENU;
	if (_stricmp(val, "RALT") == 0) return VK_RMENU;
	if (_stricmp(val, "CAPSLOCK") == 0) return VK_CAPITAL;
	if (_stricmp(val, "INSERT") == 0) return VK_INSERT;
	if (_stricmp(val, "DELETE") == 0) return VK_DELETE;
	if (_stricmp(val, "HOME") == 0) return VK_HOME;
	if (_stricmp(val, "END") == 0) return VK_END;
	if (_stricmp(val, "PAGEUP") == 0) return VK_PRIOR;
	if (_stricmp(val, "PAGEDOWN") == 0) return VK_NEXT;

	if (strlen(val) == 1 && isalpha((unsigned char)val[0]))
		return toupper((unsigned char)val[0]);

	if (strlen(val) == 1 && isdigit((unsigned char)val[0]))
		return val[0];

	return 0;
}

static void WriteDefaultINI(const char* path)
{
	FILE* f = fopen(path, "w");
	if (!f) return;
	fprintf(f, "[Keybinds]\n");
	fprintf(f, "forward=W\n");
	fprintf(f, "backward=S\n");
	fprintf(f, "left=A\n");
	fprintf(f, "right=D\n");
	fprintf(f, "jump=SPACE\n");
	fprintf(f, "sneak=LSHIFT\n");
	fprintf(f, "sprint=LCONTROL\n");
	fprintf(f, "inventory=E\n");
	fprintf(f, "drop=Q\n");
	fprintf(f, "chat=T\n");
	fprintf(f, "crafting=TAB\n");
	fprintf(f, "confirm=RETURN\n");
	fprintf(f, "pause=ESCAPE\n");
	fprintf(f, "thirdperson=F5\n");
	fprintf(f, "debuginfo=F3\n");
	fprintf(f, "voice=V\n");
	fprintf(f, "\n[Voice]\n");
	fprintf(f, "; mode: pushtotalk or active\n");
	fprintf(f, "mode=pushtotalk\n");
	fclose(f);
}

KBMConfig& KBMConfig::Get()
{
	if (!s_loaded)
		s_config.Load();
	return s_config;
}

void KBMConfig::Load()
{
	keyForward = 'W';
	keyBackward = 'S';
	keyLeft = 'A';
	keyRight = 'D';
	keyJump = VK_SPACE;
	keySneak = VK_LSHIFT;
	keySprint = VK_LCONTROL;
	keyInventory = 'E';
	keyDrop = 'Q';
	keyChat = 'T';
	keyCrafting = VK_TAB;
	keyConfirm = VK_RETURN;
	keyPause = VK_ESCAPE;
	keyThirdPerson = VK_F5;
	keyDebugInfo = VK_F3;
	keyVoice = 'V';
	voiceMode = 0;
	s_loaded = true;

	char exePath[MAX_PATH];
#if defined(__linux__)
	if (getcwd(exePath, sizeof(exePath)) == NULL) return;
	size_t exeLen = strlen(exePath);
	if (exeLen + 1 >= MAX_PATH) return;
	if (exeLen > 0 && exePath[exeLen - 1] != '/')
	{
		exePath[exeLen] = '/';
		exePath[exeLen + 1] = 0;
	}
#else
	if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) return;

	char* slash = strrchr(exePath, '\\');
	if (slash) *(slash + 1) = 0;
#endif

	char iniPath[MAX_PATH];
	#if defined(__linux__)
	int written = snprintf(iniPath, MAX_PATH, "%skbm_config.ini", exePath);
	#else
	int written = _snprintf_s(iniPath, MAX_PATH, _TRUNCATE, "%skbm_config.ini", exePath);
	#endif
	if (written <= 0 || written >= MAX_PATH) return;

	FILE* f = OpenINIWithCreate(iniPath);
	if (!f)
	{
		char appDataINIPath[MAX_PATH];
		if (BuildAppDataINIPath(appDataINIPath, MAX_PATH))
			f = OpenINIWithCreate(appDataINIPath);
	}

	if (!f) return;

	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		char* nl = strchr(line, '\n');
		if (nl) *nl = 0;
		char* cr = strchr(line, '\r');
		if (cr) *cr = 0;

		if (line[0] == '[' || line[0] == ';' || line[0] == '#' || line[0] == 0)
			continue;

		char* eq = strchr(line, '=');
		if (!eq) continue;

		*eq = 0;
		char* key = line;
		char* val = eq + 1;

		while (*key == ' ' || *key == '\t') key++;
		char* kend = key + strlen(key) - 1;
		while (kend > key && (*kend == ' ' || *kend == '\t')) { *kend = 0; kend--; }

		while (*val == ' ' || *val == '\t') val++;
		char* vend = val + strlen(val) - 1;
		while (vend > val && (*vend == ' ' || *vend == '\t')) { *vend = 0; vend--; }

		if (_stricmp(key, "forward") == 0) { int v = ParseVK(val); if (v) keyForward = v; }
		else if (_stricmp(key, "backward") == 0) { int v = ParseVK(val); if (v) keyBackward = v; }
		else if (_stricmp(key, "left") == 0) { int v = ParseVK(val); if (v) keyLeft = v; }
		else if (_stricmp(key, "right") == 0) { int v = ParseVK(val); if (v) keyRight = v; }
		else if (_stricmp(key, "jump") == 0) { int v = ParseVK(val); if (v) keyJump = v; }
		else if (_stricmp(key, "sneak") == 0) { int v = ParseVK(val); if (v) keySneak = v; }
		else if (_stricmp(key, "sprint") == 0) { int v = ParseVK(val); if (v) keySprint = v; }
		else if (_stricmp(key, "inventory") == 0) { int v = ParseVK(val); if (v) keyInventory = v; }
		else if (_stricmp(key, "drop") == 0) { int v = ParseVK(val); if (v) keyDrop = v; }
		else if (_stricmp(key, "chat") == 0) { int v = ParseVK(val); if (v) keyChat = v; }
		else if (_stricmp(key, "crafting") == 0) { int v = ParseVK(val); if (v) keyCrafting = v; }
		else if (_stricmp(key, "confirm") == 0) { int v = ParseVK(val); if (v) keyConfirm = v; }
		else if (_stricmp(key, "pause") == 0) { int v = ParseVK(val); if (v) keyPause = v; }
		else if (_stricmp(key, "thirdperson") == 0) { int v = ParseVK(val); if (v) keyThirdPerson = v; }
		else if (_stricmp(key, "debuginfo") == 0) { int v = ParseVK(val); if (v) keyDebugInfo = v; }
		else if (_stricmp(key, "voice") == 0) { int v = ParseVK(val); if (v) keyVoice = v; }
		else if (_stricmp(key, "mode") == 0)
		{
			if (_stricmp(val, "active") == 0) voiceMode = 1;
			else voiceMode = 0;
		}
	}
	fclose(f);
}

#endif
