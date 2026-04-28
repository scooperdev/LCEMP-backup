#include "stdafx.h"

#ifdef _WINDOWS64

#include "VoiceChat.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "../Network/WinsockNetLayer.h"
#include "../../../Minecraft.World/CustomPayloadPacket.h"
#include "../../../Minecraft.World/ArrayWithLength.h"
#include "../../Minecraft.h"
#include "../../MultiPlayerLocalPlayer.h"
#include "../../ClientConnection.h"
#include "../../KeyboardMouseInput.h"
#include "../KBMConfig.h"

#include <cmath>

#define VOICE_SAMPLE_RATE 16000
#define VOICE_BITS 16
#define VOICE_CHANNELS 1
#define VOICE_FRAME_MS 60
#define VOICE_FRAME_SAMPLES (VOICE_SAMPLE_RATE * VOICE_FRAME_MS / 1000)
#define VOICE_FRAME_BYTES (VOICE_FRAME_SAMPLES * (VOICE_BITS / 8) * VOICE_CHANNELS)
#define VOICE_CAPTURE_BUFFERS 6
#define VOICE_PLAY_BUFFERS 16
#define VOICE_MAX_PLAYERS 8
#define VOICE_SILENCE_THRESHOLD 500
#define VOICE_TALKING_TIMEOUT_MS 500

static const wstring VOICE_CHANNEL = L"MC|Voice";

static WAVEFORMATEX s_format;
static HWAVEIN s_waveIn = NULL;
static WAVEHDR s_captureHeaders[VOICE_CAPTURE_BUFFERS];
static char s_captureBuffers[VOICE_CAPTURE_BUFFERS][VOICE_FRAME_BYTES];
static volatile long s_captureReady[VOICE_CAPTURE_BUFFERS];

static HWAVEOUT s_playOut[VOICE_MAX_PLAYERS];
static WAVEHDR s_playHeaders[VOICE_MAX_PLAYERS][VOICE_PLAY_BUFFERS];
static char s_playData[VOICE_MAX_PLAYERS][VOICE_PLAY_BUFFERS][VOICE_FRAME_BYTES];
static int s_nextPlayBuffer[VOICE_MAX_PLAYERS];

static bool s_initialized = false;
static bool s_enabled = false;
static bool s_micOpen = false;
static DWORD s_talkingTime[VOICE_MAX_PLAYERS];
static bool s_localTalking = false;

static void CALLBACK voiceCaptureProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WIM_DATA)
	{
		WAVEHDR *hdr = (WAVEHDR *)dwParam1;
		int idx = (int)(hdr - s_captureHeaders);
		if (idx >= 0 && idx < VOICE_CAPTURE_BUFFERS)
			InterlockedExchange(&s_captureReady[idx], 1);
	}
}

static bool isAudioActive(const char *data, int size)
{
	const short *samples = (const short *)data;
	int count = size / 2;
	long long sum = 0;
	for (int i = 0; i < count; i++)
	{
		int v = samples[i];
		sum += (long long)v * v;
	}
	double rms = sqrt((double)sum / count);
	return rms > VOICE_SILENCE_THRESHOLD;
}

bool VoiceChat::init()
{
	if (s_initialized) return true;

	memset(&s_format, 0, sizeof(s_format));
	s_format.wFormatTag = WAVE_FORMAT_PCM;
	s_format.nChannels = VOICE_CHANNELS;
	s_format.nSamplesPerSec = VOICE_SAMPLE_RATE;
	s_format.wBitsPerSample = VOICE_BITS;
	s_format.nBlockAlign = s_format.nChannels * s_format.wBitsPerSample / 8;
	s_format.nAvgBytesPerSec = s_format.nSamplesPerSec * s_format.nBlockAlign;

	memset((void*)s_captureReady, 0, sizeof(s_captureReady));
	memset(s_playOut, 0, sizeof(s_playOut));
	memset(s_nextPlayBuffer, 0, sizeof(s_nextPlayBuffer));
	memset(s_talkingTime, 0, sizeof(s_talkingTime));

	s_initialized = true;
	return true;
}

bool VoiceChat::openMicrophone()
{
	if (s_micOpen) return true;

	UINT devCount = waveInGetNumDevs();
	if (devCount == 0) return false;

	MMRESULT result = waveInOpen(&s_waveIn, WAVE_MAPPER, &s_format,
		(DWORD_PTR)voiceCaptureProc, 0, CALLBACK_FUNCTION);
	if (result != MMSYSERR_NOERROR) return false;

	for (int i = 0; i < VOICE_CAPTURE_BUFFERS; i++)
	{
		memset(&s_captureHeaders[i], 0, sizeof(WAVEHDR));
		s_captureHeaders[i].lpData = s_captureBuffers[i];
		s_captureHeaders[i].dwBufferLength = VOICE_FRAME_BYTES;
		waveInPrepareHeader(s_waveIn, &s_captureHeaders[i], sizeof(WAVEHDR));
		waveInAddBuffer(s_waveIn, &s_captureHeaders[i], sizeof(WAVEHDR));
		s_captureReady[i] = 0;
	}

	waveInStart(s_waveIn);
	s_micOpen = true;
	return true;
}

void VoiceChat::closeMicrophone()
{
	if (!s_micOpen) return;

	waveInStop(s_waveIn);
	waveInReset(s_waveIn);

	for (int i = 0; i < VOICE_CAPTURE_BUFFERS; i++)
		waveInUnprepareHeader(s_waveIn, &s_captureHeaders[i], sizeof(WAVEHDR));

	waveInClose(s_waveIn);
	s_waveIn = NULL;
	s_micOpen = false;
}

bool VoiceChat::openPlayback(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= VOICE_MAX_PLAYERS) return false;
	if (s_playOut[playerIndex]) return true;

	MMRESULT result = waveOutOpen(&s_playOut[playerIndex], WAVE_MAPPER, &s_format,
		0, 0, CALLBACK_NULL);
	if (result != MMSYSERR_NOERROR) return false;

	memset(s_playHeaders[playerIndex], 0, sizeof(WAVEHDR) * VOICE_PLAY_BUFFERS);
	s_nextPlayBuffer[playerIndex] = 0;
	return true;
}

void VoiceChat::closePlayback(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= VOICE_MAX_PLAYERS) return;
	if (!s_playOut[playerIndex]) return;

	waveOutReset(s_playOut[playerIndex]);

	for (int i = 0; i < VOICE_PLAY_BUFFERS; i++)
	{
		if (s_playHeaders[playerIndex][i].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(s_playOut[playerIndex], &s_playHeaders[playerIndex][i], sizeof(WAVEHDR));
	}

	waveOutClose(s_playOut[playerIndex]);
	s_playOut[playerIndex] = NULL;
}

void VoiceChat::shutdown()
{
	if (!s_initialized) return;
	closeMicrophone();
	for (int i = 0; i < VOICE_MAX_PLAYERS; i++)
		closePlayback(i);
	s_initialized = false;
	s_enabled = false;
}

void VoiceChat::tick()
{
	if (!s_initialized || !s_enabled) return;

	Minecraft *mc = Minecraft::GetInstance();
	if (!mc || !mc->localplayers[0] || !mc->localplayers[0]->connection) return;

	if (!s_micOpen)
		openMicrophone();

	if (!s_micOpen) return;

	KBMConfig& cfg = KBMConfig::Get();
	bool pttHeld = (cfg.voiceMode == 1) || !g_KBMInput.IsKBMActive() || g_KBMInput.IsKeyDown(cfg.keyVoice);

	s_localTalking = false;
	for (int i = 0; i < VOICE_CAPTURE_BUFFERS; i++)
	{
		if (InterlockedCompareExchange(&s_captureReady[i], 0, 1) == 1)
		{
			if (pttHeld && isAudioActive(s_captureBuffers[i], VOICE_FRAME_BYTES))
			{
				s_localTalking = true;

				byteArray voiceData;
				voiceData.data = new byte[VOICE_FRAME_BYTES];
				voiceData.length = VOICE_FRAME_BYTES;
				memcpy(voiceData.data, s_captureBuffers[i], VOICE_FRAME_BYTES);

				mc->localplayers[0]->connection->send(
					shared_ptr<CustomPayloadPacket>(new CustomPayloadPacket(VOICE_CHANNEL, voiceData)));
			}

			waveInAddBuffer(s_waveIn, &s_captureHeaders[i], sizeof(WAVEHDR));
		}
	}
}

void VoiceChat::onVoiceReceived(unsigned char senderSmallId, const char *data, int dataSize)
{
	if (!s_initialized || !s_enabled) return;
	if (senderSmallId >= VOICE_MAX_PLAYERS) return;
	if (dataSize <= 0 || dataSize > VOICE_FRAME_BYTES) return;

	if (!openPlayback(senderSmallId)) return;

	s_talkingTime[senderSmallId] = GetTickCount();

	int bufIdx = s_nextPlayBuffer[senderSmallId];
	WAVEHDR *hdr = &s_playHeaders[senderSmallId][bufIdx];

	if (hdr->dwFlags & WHDR_PREPARED)
	{
		if (!(hdr->dwFlags & WHDR_DONE))
			return;
		waveOutUnprepareHeader(s_playOut[senderSmallId], hdr, sizeof(WAVEHDR));
	}

	memcpy(s_playData[senderSmallId][bufIdx], data, dataSize);

	memset(hdr, 0, sizeof(WAVEHDR));
	hdr->lpData = s_playData[senderSmallId][bufIdx];
	hdr->dwBufferLength = dataSize;

	waveOutPrepareHeader(s_playOut[senderSmallId], hdr, sizeof(WAVEHDR));
	waveOutWrite(s_playOut[senderSmallId], hdr, sizeof(WAVEHDR));

	s_nextPlayBuffer[senderSmallId] = (bufIdx + 1) % VOICE_PLAY_BUFFERS;
}

bool VoiceChat::isTalking(unsigned char smallId)
{
	if (!s_initialized || !s_enabled) return false;

	if (smallId == WinsockNetLayer::GetLocalSmallId())
		return s_localTalking;

	if (smallId >= VOICE_MAX_PLAYERS) return false;

	DWORD now = GetTickCount();
	return (s_talkingTime[smallId] != 0 && (now - s_talkingTime[smallId]) < VOICE_TALKING_TIMEOUT_MS);
}

bool VoiceChat::hasVoice()
{
	return s_initialized && s_enabled;
}

void VoiceChat::setEnabled(bool enabled)
{
	s_enabled = enabled;
	if (!enabled)
	{
		closeMicrophone();
		for (int i = 0; i < VOICE_MAX_PLAYERS; i++)
			closePlayback(i);
	}
}

bool VoiceChat::isEnabled()
{
	return s_enabled;
}

#endif
