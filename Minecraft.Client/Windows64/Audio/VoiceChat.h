#pragma once

#ifdef _WINDOWS64

class VoiceChat
{
public:
	static bool init();
	static void shutdown();
	static void tick();
	static void onVoiceReceived(unsigned char senderSmallId, const char *data, int dataSize);
	static bool isTalking(unsigned char smallId);
	static bool hasVoice();
	static void setEnabled(bool enabled);
	static bool isEnabled();

private:
	static bool openMicrophone();
	static void closeMicrophone();
	static bool openPlayback(int playerIndex);
	static void closePlayback(int playerIndex);
};

#endif
