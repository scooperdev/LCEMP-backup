// Code implemented by LCEMP, credit if used on other repos

#include "stdafx.h"

#ifdef _WINDOWS64

#include "WinsockNetLayer.h"
#include "../../Common/Network/PlatformNetworkManagerStub.h"
#include "../../../Minecraft.World/Socket.h"

SOCKET WinsockNetLayer::s_listenSocket = INVALID_SOCKET;
SOCKET WinsockNetLayer::s_hostConnectionSocket = INVALID_SOCKET;
HANDLE WinsockNetLayer::s_acceptThread = NULL;
HANDLE WinsockNetLayer::s_ioThread = NULL;
HANDLE WinsockNetLayer::s_clientRecvThread = NULL;

bool WinsockNetLayer::s_isHost = false;
bool WinsockNetLayer::s_connected = false;
bool WinsockNetLayer::s_active = false;
bool WinsockNetLayer::s_initialized = false;

BYTE WinsockNetLayer::s_localSmallId = 0;
BYTE WinsockNetLayer::s_hostSmallId = 0;
BYTE WinsockNetLayer::s_nextSmallId = 1;

CRITICAL_SECTION WinsockNetLayer::s_sendLock;
CRITICAL_SECTION WinsockNetLayer::s_connectionsLock;

std::vector<Win64RemoteConnection> WinsockNetLayer::s_connections;

SOCKET WinsockNetLayer::s_advertiseSock = INVALID_SOCKET;
HANDLE WinsockNetLayer::s_advertiseThread = NULL;
volatile bool WinsockNetLayer::s_advertising = false;
Win64LANBroadcast WinsockNetLayer::s_advertiseData = {};
CRITICAL_SECTION WinsockNetLayer::s_advertiseLock;
int WinsockNetLayer::s_hostGamePort = WIN64_NET_DEFAULT_PORT;

SOCKET WinsockNetLayer::s_discoverySock = INVALID_SOCKET;
SOCKET WinsockNetLayer::s_discoveryLegacySock = INVALID_SOCKET;
HANDLE WinsockNetLayer::s_discoveryThread = NULL;
volatile bool WinsockNetLayer::s_discovering = false;
CRITICAL_SECTION WinsockNetLayer::s_discoveryLock;
std::vector<Win64LANSession> WinsockNetLayer::s_discoveredSessions;

CRITICAL_SECTION WinsockNetLayer::s_disconnectLock;
std::vector<BYTE> WinsockNetLayer::s_disconnectedSmallIds;

CRITICAL_SECTION WinsockNetLayer::s_pendingJoinLock;
std::vector<BYTE> WinsockNetLayer::s_pendingJoinSmallIds;

CRITICAL_SECTION WinsockNetLayer::s_freeSmallIdLock;
std::vector<BYTE> WinsockNetLayer::s_freeSmallIds;

CRITICAL_SECTION WinsockNetLayer::s_earlyDataLock;
std::vector<std::vector<BYTE> > WinsockNetLayer::s_earlyDataBuffers;

HANDLE WinsockNetLayer::s_asyncJoinThread = NULL;
volatile WinsockNetLayer::JoinResult WinsockNetLayer::s_asyncJoinResult = WinsockNetLayer::JOIN_FAILED;
volatile bool WinsockNetLayer::s_asyncJoinActive = false;
char WinsockNetLayer::s_asyncJoinIP[256] = {};
int WinsockNetLayer::s_asyncJoinPort = 0;

bool g_Win64MultiplayerHost = false;
bool g_Win64MultiplayerJoin = false;
int g_Win64MultiplayerPort = WIN64_NET_DEFAULT_PORT;
char g_Win64MultiplayerIP[256] = "127.0.0.1";

bool g_ServerAdvertiseLAN = true;
char g_ServerBindAddress[256] = "";
#ifdef _DEDICATED_SERVER
int g_ServerMaxPlayers = MINECRAFT_NET_MAX_PLAYERS;
#else
int g_ServerMaxPlayers = 8;
#endif

size_t WinsockNetLayer::GetConnectionSlotCount()
{
	int configured = g_ServerMaxPlayers;
	if (configured < 1)
		configured = 1;
	if (configured > 255)
		configured = 255;
	return (size_t)configured;
}

bool WinsockNetLayer::Initialize()
{
	if (s_initialized) return true;

	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		app.DebugPrintf("WSAStartup failed: %d\n", result);
		return false;
	}

	InitializeCriticalSection(&s_sendLock);
	InitializeCriticalSection(&s_connectionsLock);
	InitializeCriticalSection(&s_advertiseLock);
	InitializeCriticalSection(&s_discoveryLock);
	InitializeCriticalSection(&s_disconnectLock);
	InitializeCriticalSection(&s_pendingJoinLock);
	InitializeCriticalSection(&s_freeSmallIdLock);
	InitializeCriticalSection(&s_earlyDataLock);

	size_t slotCount = GetConnectionSlotCount();
	s_connections.clear();
	s_connections.resize(slotCount);
	s_earlyDataBuffers.clear();
	s_earlyDataBuffers.resize(slotCount);

	for (size_t i = 0; i < slotCount; i++)
	{
		s_connections[i].tcpSocket = INVALID_SOCKET;
		s_connections[i].smallId = (BYTE)i;
		s_connections[i].active = false;
		InitializeCriticalSection(&s_connections[i].sendLock);
		s_connections[i].recvBuffer = NULL;
		s_connections[i].recvBufferUsed = 0;
		s_connections[i].recvBufferSize = 0;
		s_connections[i].currentPacketSize = 0;
		s_connections[i].readingHeader = true;
		s_connections[i].sendBuffer = NULL;
		s_connections[i].sendBufferUsed = 0;
		s_connections[i].sendBufferSize = 0;
		InitializeCriticalSection(&s_connections[i].sendBufLock);
	}

	s_initialized = true;

#ifndef WITH_SERVER_CODE
	StartDiscovery();
#endif

	return true;
}

void WinsockNetLayer::Shutdown()
{
	CancelJoinGame();
	StopAdvertising();
	StopDiscovery();

	s_active = false;
	s_connected = false;

	if (s_listenSocket != INVALID_SOCKET)
	{
		closesocket(s_listenSocket);
		s_listenSocket = INVALID_SOCKET;
	}

	if (s_hostConnectionSocket != INVALID_SOCKET)
	{
		closesocket(s_hostConnectionSocket);
		s_hostConnectionSocket = INVALID_SOCKET;
	}

	EnterCriticalSection(&s_connectionsLock);
	for (size_t i = 0; i < s_connections.size(); i++)
	{
		s_connections[i].active = false;
		if (s_connections[i].tcpSocket != INVALID_SOCKET)
		{
			closesocket(s_connections[i].tcpSocket);
			s_connections[i].tcpSocket = INVALID_SOCKET;
		}
		if (s_connections[i].recvBuffer != NULL)
		{
			free(s_connections[i].recvBuffer);
			s_connections[i].recvBuffer = NULL;
		}
		if (s_connections[i].sendBuffer != NULL)
		{
			free(s_connections[i].sendBuffer);
			s_connections[i].sendBuffer = NULL;
		}
		DeleteCriticalSection(&s_connections[i].sendBufLock);
		DeleteCriticalSection(&s_connections[i].sendLock);
	}
	s_connections.clear();
	s_earlyDataBuffers.clear();
	LeaveCriticalSection(&s_connectionsLock);

	if (s_ioThread != NULL)
	{
		WaitForSingleObject(s_ioThread, 3000);
		CloseHandle(s_ioThread);
		s_ioThread = NULL;
	}

	if (s_acceptThread != NULL)
	{
		WaitForSingleObject(s_acceptThread, 2000);
		CloseHandle(s_acceptThread);
		s_acceptThread = NULL;
	}

	if (s_clientRecvThread != NULL)
	{
		WaitForSingleObject(s_clientRecvThread, 2000);
		CloseHandle(s_clientRecvThread);
		s_clientRecvThread = NULL;
	}

	if (s_initialized)
	{
		DeleteCriticalSection(&s_sendLock);
		DeleteCriticalSection(&s_connectionsLock);
		DeleteCriticalSection(&s_advertiseLock);
		DeleteCriticalSection(&s_discoveryLock);
		DeleteCriticalSection(&s_disconnectLock);
		s_disconnectedSmallIds.clear();
		DeleteCriticalSection(&s_pendingJoinLock);
		s_pendingJoinSmallIds.clear();
		DeleteCriticalSection(&s_freeSmallIdLock);
		s_freeSmallIds.clear();
		WSACleanup();
		s_initialized = false;
	}
}

bool WinsockNetLayer::HostGame(int port)
{
	if (!s_initialized && !Initialize()) return false;

	s_isHost = true;
	s_localSmallId = 0;
	s_hostSmallId = 0;
	s_nextSmallId = 1;
	s_hostGamePort = port;

	EnterCriticalSection(&s_freeSmallIdLock);
	s_freeSmallIds.clear();
	LeaveCriticalSection(&s_freeSmallIdLock);

	struct addrinfo hints = {};
	struct addrinfo *result = NULL;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	char portStr[16];
	sprintf_s(portStr, "%d", port);

	const char *bindNode = NULL;
	extern char g_ServerBindAddress[256];
	if (g_ServerBindAddress[0] != '\0')
	{
		bindNode = g_ServerBindAddress;
		hints.ai_flags = 0;
	}

	int iResult = getaddrinfo(bindNode, portStr, &hints, &result);
	if (iResult != 0)
	{
		app.DebugPrintf("getaddrinfo failed: %d\n", iResult);
		return false;
	}

	s_listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (s_listenSocket == INVALID_SOCKET)
	{
		app.DebugPrintf("socket() failed: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		return false;
	}

	int opt = 1;
	setsockopt(s_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

	iResult = ::bind(s_listenSocket, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
	if (iResult == SOCKET_ERROR)
	{
		app.DebugPrintf("bind() failed: %d\n", WSAGetLastError());
		closesocket(s_listenSocket);
		s_listenSocket = INVALID_SOCKET;
		return false;
	}

	iResult = listen(s_listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		app.DebugPrintf("listen() failed: %d\n", WSAGetLastError());
		closesocket(s_listenSocket);
		s_listenSocket = INVALID_SOCKET;
		return false;
	}

	s_active = true;
	s_connected = true;

	s_ioThread = CreateThread(NULL, 0, IOThreadProc, NULL, 0, NULL);
	s_acceptThread = CreateThread(NULL, 0, AcceptThreadProc, NULL, 0, NULL);

	app.DebugPrintf("Win64 LAN: Hosting on port %d\n", port);
	return true;
}

bool WinsockNetLayer::JoinGame(const char *ip, int port)
{
	if (!s_initialized && !Initialize()) return false;

	s_isHost = false;
	s_hostSmallId = 0;
	s_connected = false;
	s_active = false;

	if (s_hostConnectionSocket != INVALID_SOCKET)
	{
		closesocket(s_hostConnectionSocket);
		s_hostConnectionSocket = INVALID_SOCKET;
	}

	struct addrinfo hints = {};
	struct addrinfo *result = NULL;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char portStr[16];
	sprintf_s(portStr, "%d", port);

	int iResult = getaddrinfo(ip, portStr, &hints, &result);
	if (iResult != 0)
	{
		app.DebugPrintf("getaddrinfo failed for %s:%d - %d\n", ip, port, iResult);
		return false;
	}

	bool connected = false;
	BYTE assignedSmallId = 0;
	const int maxAttempts = 3;
	const int connectTimeoutMs = 3000;

	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		s_hostConnectionSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (s_hostConnectionSocket == INVALID_SOCKET)
		{
			app.DebugPrintf("socket() failed: %d\n", WSAGetLastError());
			break;
		}

		int noDelay = 1;
		setsockopt(s_hostConnectionSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

		u_long nonBlocking = 1;
		ioctlsocket(s_hostConnectionSocket, FIONBIO, &nonBlocking);

		iResult = connect(s_hostConnectionSocket, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
			{
				fd_set writeFds, exceptFds;
				FD_ZERO(&writeFds);
				FD_ZERO(&exceptFds);
				FD_SET(s_hostConnectionSocket, &writeFds);
				FD_SET(s_hostConnectionSocket, &exceptFds);

				struct timeval tv;
				tv.tv_sec = connectTimeoutMs / 1000;
				tv.tv_usec = (connectTimeoutMs % 1000) * 1000;

				int selectResult = select(0, NULL, &writeFds, &exceptFds, &tv);
				if (selectResult <= 0 || FD_ISSET(s_hostConnectionSocket, &exceptFds))
				{
					app.DebugPrintf("connect() to %s:%d timed out or failed (attempt %d/%d)\n", ip, port, attempt + 1, maxAttempts);
					closesocket(s_hostConnectionSocket);
					s_hostConnectionSocket = INVALID_SOCKET;
					continue;
				}

				int sockErr = 0;
				int sockErrLen = sizeof(sockErr);
				getsockopt(s_hostConnectionSocket, SOL_SOCKET, SO_ERROR, (char *)&sockErr, &sockErrLen);
				if (sockErr != 0)
				{
					app.DebugPrintf("connect() to %s:%d failed with SO_ERROR %d (attempt %d/%d)\n", ip, port, sockErr, attempt + 1, maxAttempts);
					closesocket(s_hostConnectionSocket);
					s_hostConnectionSocket = INVALID_SOCKET;
					continue;
				}
			}
			else
			{
				app.DebugPrintf("connect() to %s:%d failed (attempt %d/%d): %d\n", ip, port, attempt + 1, maxAttempts, err);
				closesocket(s_hostConnectionSocket);
				s_hostConnectionSocket = INVALID_SOCKET;
				continue;
			}
		}

		u_long blocking = 0;
		ioctlsocket(s_hostConnectionSocket, FIONBIO, &blocking);

		DWORD recvTimeout = 3000;
		setsockopt(s_hostConnectionSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recvTimeout, sizeof(recvTimeout));

		BYTE assignBuf[1];
		int bytesRecv = recv(s_hostConnectionSocket, (char *)assignBuf, 1, 0);
		if (bytesRecv != 1)
		{
			app.DebugPrintf("Failed to receive small ID assignment from host (attempt %d/%d)\n", attempt + 1, maxAttempts);
			closesocket(s_hostConnectionSocket);
			s_hostConnectionSocket = INVALID_SOCKET;
			continue;
		}

		assignedSmallId = assignBuf[0];
		connected = true;
		break;
	}
	freeaddrinfo(result);

	if (!connected)
	{
		return false;
	}
	s_localSmallId = assignedSmallId;

	DWORD noTimeout = 0;
	setsockopt(s_hostConnectionSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&noTimeout, sizeof(noTimeout));

	app.DebugPrintf("Win64 LAN: Connected to %s:%d, assigned smallId=%d\n", ip, port, s_localSmallId);

	s_active = true;
	s_connected = true;

	s_clientRecvThread = CreateThread(NULL, 0, ClientRecvThreadProc, NULL, 0, NULL);

	return true;
}

bool WinsockNetLayer::BeginJoinGame(const char *ip, int port)
{
	if (s_asyncJoinActive)
		return false;

	CancelJoinGame();

	strncpy_s(s_asyncJoinIP, sizeof(s_asyncJoinIP), ip, _TRUNCATE);
	s_asyncJoinPort = port;
	s_asyncJoinResult = JOIN_IN_PROGRESS;
	s_asyncJoinActive = true;

	s_asyncJoinThread = CreateThread(NULL, 0, AsyncJoinThreadProc, NULL, 0, NULL);
	if (s_asyncJoinThread == NULL)
	{
		s_asyncJoinActive = false;
		s_asyncJoinResult = JOIN_FAILED;
		return false;
	}

	return true;
}

WinsockNetLayer::JoinResult WinsockNetLayer::PollJoinResult()
{
	return s_asyncJoinResult;
}

void WinsockNetLayer::CancelJoinGame()
{
	if (s_asyncJoinThread != NULL)
	{
		s_asyncJoinActive = false;
		WaitForSingleObject(s_asyncJoinThread, 5000);
		CloseHandle(s_asyncJoinThread);
		s_asyncJoinThread = NULL;
	}

	if (s_hostConnectionSocket != INVALID_SOCKET && !s_connected)
	{
		closesocket(s_hostConnectionSocket);
		s_hostConnectionSocket = INVALID_SOCKET;
	}
}

DWORD WINAPI WinsockNetLayer::AsyncJoinThreadProc(LPVOID param)
{
	bool result = JoinGame(s_asyncJoinIP, s_asyncJoinPort);
	s_asyncJoinResult = result ? JOIN_SUCCESS : JOIN_FAILED;
	s_asyncJoinActive = false;
	return 0;
}

bool WinsockNetLayer::SendOnSocket(SOCKET sock, const void *data, int dataSize)
{
	if (sock == INVALID_SOCKET || dataSize <= 0 || dataSize > WIN64_NET_MAX_PACKET_SIZE) return false;

	BYTE header[4];
	header[0] = (BYTE)((dataSize >> 24) & 0xFF);
	header[1] = (BYTE)((dataSize >> 16) & 0xFF);
	header[2] = (BYTE)((dataSize >> 8) & 0xFF);
	header[3] = (BYTE)(dataSize & 0xFF);

	int totalSent = 0;
	int toSend = 4;
	while (totalSent < toSend)
	{
		int sent = send(sock, (const char *)header + totalSent, toSend - totalSent, 0);
		if (sent == SOCKET_ERROR || sent == 0)
			return false;
		totalSent += sent;
	}

	totalSent = 0;
	while (totalSent < dataSize)
	{
		int sent = send(sock, (const char *)data + totalSent, dataSize - totalSent, 0);
		if (sent == SOCKET_ERROR || sent == 0)
			return false;
		totalSent += sent;
	}

	return true;
}

bool WinsockNetLayer::SendToSmallId(BYTE targetSmallId, const void *data, int dataSize)
{
	if (!s_active) return false;
	if (dataSize <= 0 || dataSize > WIN64_NET_MAX_PACKET_SIZE || data == NULL) return false;

	if (s_isHost)
	{
		if ((size_t)targetSmallId >= s_connections.size()) return false;

		EnterCriticalSection(&s_connectionsLock);
		if (!s_connections[targetSmallId].active)
		{
			LeaveCriticalSection(&s_connectionsLock);
			return false;
		}
		LeaveCriticalSection(&s_connectionsLock);

		BYTE header[4];
		header[0] = (BYTE)((dataSize >> 24) & 0xFF);
		header[1] = (BYTE)((dataSize >> 16) & 0xFF);
		header[2] = (BYTE)((dataSize >> 8) & 0xFF);
		header[3] = (BYTE)(dataSize & 0xFF);

		int totalNeeded = 4 + dataSize;

		EnterCriticalSection(&s_connections[targetSmallId].sendBufLock);

		if (s_connections[targetSmallId].sendBufferUsed + totalNeeded > WIN64_NET_MAX_SEND_QUEUE)
		{
			LeaveCriticalSection(&s_connections[targetSmallId].sendBufLock);
			app.DebugPrintf("Win64 LAN: Send queue overflow for smallId=%d, disconnecting\n", targetSmallId);
			CloseConnectionBySmallId(targetSmallId);
			return false;
		}

		int space = s_connections[targetSmallId].sendBufferSize - s_connections[targetSmallId].sendBufferUsed;
		if (space < totalNeeded)
		{
			int newSize = s_connections[targetSmallId].sendBufferUsed + totalNeeded + 65536;
			BYTE *newBuf = (BYTE *)realloc(s_connections[targetSmallId].sendBuffer, newSize);
			if (newBuf == NULL)
			{
				LeaveCriticalSection(&s_connections[targetSmallId].sendBufLock);
				return false;
			}
			s_connections[targetSmallId].sendBuffer = newBuf;
			s_connections[targetSmallId].sendBufferSize = newSize;
		}

		memcpy(s_connections[targetSmallId].sendBuffer + s_connections[targetSmallId].sendBufferUsed, header, 4);
		s_connections[targetSmallId].sendBufferUsed += 4;
		memcpy(s_connections[targetSmallId].sendBuffer + s_connections[targetSmallId].sendBufferUsed, data, dataSize);
		s_connections[targetSmallId].sendBufferUsed += dataSize;

		LeaveCriticalSection(&s_connections[targetSmallId].sendBufLock);
		return true;
	}
	else
	{
		EnterCriticalSection(&s_sendLock);
		bool result = SendOnSocket(s_hostConnectionSocket, data, dataSize);
		LeaveCriticalSection(&s_sendLock);
		return result;
	}
}

SOCKET WinsockNetLayer::GetSocketForSmallId(BYTE smallId)
{
	EnterCriticalSection(&s_connectionsLock);
	if ((size_t)smallId < s_connections.size() && s_connections[smallId].active)
	{
		SOCKET sock = s_connections[smallId].tcpSocket;
		LeaveCriticalSection(&s_connectionsLock);
		return sock;
	}
	LeaveCriticalSection(&s_connectionsLock);
	return INVALID_SOCKET;
}

std::string WinsockNetLayer::GetIPForSmallId(BYTE smallId)
{
	SOCKET sock = GetSocketForSmallId(smallId);
	if (sock == INVALID_SOCKET) return "";
	struct sockaddr_in addr;
	int addrLen = sizeof(addr);
	if (getpeername(sock, (struct sockaddr *)&addr, &addrLen) != 0) return "";
	char buf[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)) == NULL) return "";
	return std::string(buf);
}

static bool RecvExact(SOCKET sock, BYTE *buf, int len)
{
	int totalRecv = 0;
	while (totalRecv < len)
	{
		int r = recv(sock, (char *)buf + totalRecv, len - totalRecv, 0);
		if (r <= 0) return false;
		totalRecv += r;
	}
	return true;
}

void WinsockNetLayer::HandleDataReceived(BYTE fromSmallId, BYTE toSmallId, unsigned char *data, unsigned int dataSize)
{
	INetworkPlayer *pPlayerFrom = g_NetworkManager.GetPlayerBySmallId(fromSmallId);
	INetworkPlayer *pPlayerTo = g_NetworkManager.GetPlayerBySmallId(toSmallId);

	if (pPlayerFrom == NULL || pPlayerTo == NULL)
	{
		if (s_isHost && fromSmallId > 0 && (size_t)fromSmallId < s_earlyDataBuffers.size())
		{
			EnterCriticalSection(&s_earlyDataLock);
			s_earlyDataBuffers[fromSmallId].insert(
				s_earlyDataBuffers[fromSmallId].end(), data, data + dataSize);
			LeaveCriticalSection(&s_earlyDataLock);
		}
		return;
	}

	if (s_isHost)
	{
		::Socket *pSocket = pPlayerFrom->GetSocket();
		if (pSocket != NULL)
			pSocket->pushDataToQueue(data, dataSize, false);
		else
		{
			EnterCriticalSection(&s_earlyDataLock);
			s_earlyDataBuffers[fromSmallId].insert(
				s_earlyDataBuffers[fromSmallId].end(), data, data + dataSize);
			LeaveCriticalSection(&s_earlyDataLock);
		}
	}
	else
	{
		::Socket *pSocket = pPlayerTo->GetSocket();
		if (pSocket != NULL)
			pSocket->pushDataToQueue(data, dataSize, true);
	}
}

void WinsockNetLayer::FlushPendingData()
{
	EnterCriticalSection(&s_earlyDataLock);
	for (size_t i = 1; i < s_earlyDataBuffers.size(); i++)
	{
		if (s_earlyDataBuffers[i].empty()) continue;

		INetworkPlayer *pPlayer = g_NetworkManager.GetPlayerBySmallId((BYTE)i);
		if (pPlayer == NULL) continue;

		::Socket *pSocket = pPlayer->GetSocket();
		if (pSocket == NULL) continue;

		pSocket->pushDataToQueue(s_earlyDataBuffers[i].data(),
			(DWORD)s_earlyDataBuffers[i].size(), false);
		s_earlyDataBuffers[i].clear();
	}
	LeaveCriticalSection(&s_earlyDataLock);
}

DWORD WINAPI WinsockNetLayer::AcceptThreadProc(LPVOID param)
{
	while (s_active)
	{
		SOCKET clientSocket = accept(s_listenSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET)
		{
			if (s_active)
				app.DebugPrintf("accept() failed: %d\n", WSAGetLastError());
			break;
		}

		int noDelay = 1;
		setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

		int sndbuf = 131072;
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sizeof(sndbuf));
		int rcvbuf = 131072;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(rcvbuf));

		extern QNET_STATE _iQNetStubState;
		if (_iQNetStubState != QNET_STATE_GAME_PLAY)
		{
			app.DebugPrintf("Win64 LAN: Rejecting connection, game not ready\n");
			closesocket(clientSocket);
			continue;
		}

		BYTE assignedSmallId;
		EnterCriticalSection(&s_freeSmallIdLock);
		if (!s_freeSmallIds.empty())
		{
			assignedSmallId = s_freeSmallIds.back();
			s_freeSmallIds.pop_back();
		}
		else if (s_nextSmallId < g_ServerMaxPlayers)
		{
			assignedSmallId = s_nextSmallId++;
		}
		else
		{
			LeaveCriticalSection(&s_freeSmallIdLock);
			app.DebugPrintf("Win64 LAN: Server full, rejecting connection\n");
			closesocket(clientSocket);
			continue;
		}
		LeaveCriticalSection(&s_freeSmallIdLock);

		if ((size_t)assignedSmallId >= s_connections.size() || assignedSmallId >= IQNet::GetPlayerCapacity())
		{
			app.DebugPrintf("Win64 LAN: Invalid smallId %d, rejecting\n", assignedSmallId);
			closesocket(clientSocket);
			continue;
		}

		BYTE assignBuf[1] = { assignedSmallId };
		int sent = send(clientSocket, (const char *)assignBuf, 1, 0);
		if (sent != 1)
		{
			app.DebugPrintf("Failed to send small ID to client\n");
			closesocket(clientSocket);
			continue;
		}

		u_long nonBlocking = 1;
		ioctlsocket(clientSocket, FIONBIO, &nonBlocking);

		Win64RemoteConnection &conn = s_connections[assignedSmallId];

		EnterCriticalSection(&s_connectionsLock);
		conn.tcpSocket = clientSocket;
		conn.smallId = assignedSmallId;
		conn.active = true;
		conn.recvBufferUsed = 0;
		conn.currentPacketSize = 0;
		conn.readingHeader = true;
		if (conn.recvBuffer == NULL)
		{
			conn.recvBuffer = (BYTE *)malloc(WIN64_NET_RECV_BUFFER_SIZE);
			conn.recvBufferSize = WIN64_NET_RECV_BUFFER_SIZE;
		}
		EnterCriticalSection(&conn.sendBufLock);
		conn.sendBufferUsed = 0;
		if (conn.sendBuffer == NULL)
		{
			conn.sendBuffer = (BYTE *)malloc(WIN64_NET_SEND_BUFFER_SIZE);
			conn.sendBufferSize = WIN64_NET_SEND_BUFFER_SIZE;
		}
		LeaveCriticalSection(&conn.sendBufLock);
		LeaveCriticalSection(&s_connectionsLock);

		app.DebugPrintf("Win64 LAN: Client connected, assigned smallId=%d\n", assignedSmallId);

		IQNetPlayer *qnetPlayer = &IQNet::m_player[assignedSmallId];

		extern void Win64_SetupRemoteQNetPlayer(IQNetPlayer *player, BYTE smallId, bool isHost, bool isLocal);
		Win64_SetupRemoteQNetPlayer(qnetPlayer, assignedSmallId, false, false);

		EnterCriticalSection(&s_pendingJoinLock);
		s_pendingJoinSmallIds.push_back(assignedSmallId);
		LeaveCriticalSection(&s_pendingJoinLock);
	}
	return 0;
}

bool WinsockNetLayer::ProcessRecvData(Win64RemoteConnection &conn)
{
	if (conn.readingHeader)
	{
		if (conn.recvBufferUsed < 4) return false;

		int packetSize =
			((uint32_t)conn.recvBuffer[0] << 24) |
			((uint32_t)conn.recvBuffer[1] << 16) |
			((uint32_t)conn.recvBuffer[2] << 8) |
			((uint32_t)conn.recvBuffer[3]);

		if (packetSize <= 0 || (unsigned int)packetSize > WIN64_NET_MAX_PACKET_SIZE)
		{
			app.DebugPrintf("Win64 LAN: Invalid packet size %d from smallId=%d\n", packetSize, conn.smallId);
			conn.active = false;
			return false;
		}

		conn.currentPacketSize = packetSize;
		conn.readingHeader = false;

		memmove(conn.recvBuffer, conn.recvBuffer + 4, conn.recvBufferUsed - 4);
		conn.recvBufferUsed -= 4;

		if (conn.recvBufferSize < packetSize)
		{
			int newSize = packetSize + 4096;
			BYTE *newBuf = (BYTE *)realloc(conn.recvBuffer, newSize);
			if (newBuf == NULL)
			{
				conn.active = false;
				return false;
			}
			conn.recvBuffer = newBuf;
			conn.recvBufferSize = newSize;
		}
	}

	if (conn.recvBufferUsed < conn.currentPacketSize) return false;

	HandleDataReceived(conn.smallId, s_hostSmallId, conn.recvBuffer, conn.currentPacketSize);

	int remaining = conn.recvBufferUsed - conn.currentPacketSize;
	if (remaining > 0)
		memmove(conn.recvBuffer, conn.recvBuffer + conn.currentPacketSize, remaining);
	conn.recvBufferUsed = remaining;
	conn.readingHeader = true;

	return (remaining >= 4);
}

DWORD WINAPI WinsockNetLayer::IOThreadProc(LPVOID param)
{
	std::vector<WSAPOLLFD> pollFds;
	std::vector<BYTE> pollSmallIds;

	while (s_active)
	{
		pollFds.clear();
		pollSmallIds.clear();

		EnterCriticalSection(&s_connectionsLock);
		for (size_t i = 1; i < s_connections.size(); i++)
		{
			if (s_connections[i].active && s_connections[i].tcpSocket != INVALID_SOCKET)
			{
				WSAPOLLFD pfd;
				pfd.fd = s_connections[i].tcpSocket;
				pfd.events = POLLIN;
				pfd.revents = 0;
				pollFds.push_back(pfd);
				pollSmallIds.push_back((BYTE)i);
			}
		}
		LeaveCriticalSection(&s_connectionsLock);

		if (pollFds.empty())
		{
			Sleep(10);
			continue;
		}

		int ret = WSAPoll(&pollFds[0], (ULONG)pollFds.size(), 50);
		if (ret <= 0) continue;

		for (size_t i = 0; i < pollFds.size(); i++)
		{
			BYTE smallId = pollSmallIds[i];
			if ((size_t)smallId >= s_connections.size()) continue;
			Win64RemoteConnection &conn = s_connections[smallId];

			if (!conn.active) continue;

			if (pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				conn.active = false;
				closesocket(conn.tcpSocket);
				conn.tcpSocket = INVALID_SOCKET;

				EnterCriticalSection(&s_disconnectLock);
				s_disconnectedSmallIds.push_back(smallId);
				LeaveCriticalSection(&s_disconnectLock);
				continue;
			}

			if (pollFds[i].revents & POLLIN)
			{
				while (conn.active)
				{
					int space = conn.recvBufferSize - conn.recvBufferUsed;
					if (space <= 0)
					{
						int newSize = conn.recvBufferSize + 65536;
						BYTE *newBuf = (BYTE *)realloc(conn.recvBuffer, newSize);
						if (newBuf == NULL)
						{
							conn.active = false;
							closesocket(conn.tcpSocket);
							conn.tcpSocket = INVALID_SOCKET;
							EnterCriticalSection(&s_disconnectLock);
							s_disconnectedSmallIds.push_back(smallId);
							LeaveCriticalSection(&s_disconnectLock);
							break;
						}
						conn.recvBuffer = newBuf;
						conn.recvBufferSize = newSize;
						space = newSize - conn.recvBufferUsed;
					}

					int bytesRead = recv(conn.tcpSocket, (char *)(conn.recvBuffer + conn.recvBufferUsed), space, 0);
					if (bytesRead > 0)
					{
						conn.recvBufferUsed += bytesRead;
						while (ProcessRecvData(conn))
							;
					}
					else if (bytesRead == 0)
					{
						conn.active = false;
						closesocket(conn.tcpSocket);
						conn.tcpSocket = INVALID_SOCKET;
						EnterCriticalSection(&s_disconnectLock);
						s_disconnectedSmallIds.push_back(smallId);
						LeaveCriticalSection(&s_disconnectLock);
						break;
					}
					else
					{
						int err = WSAGetLastError();
						if (err == WSAEWOULDBLOCK)
							break;
						conn.active = false;
						closesocket(conn.tcpSocket);
						conn.tcpSocket = INVALID_SOCKET;
						EnterCriticalSection(&s_disconnectLock);
						s_disconnectedSmallIds.push_back(smallId);
						LeaveCriticalSection(&s_disconnectLock);
						break;
					}
				}
			}
		}
	}
	return 0;
}

bool WinsockNetLayer::PopDisconnectedSmallId(BYTE *outSmallId)
{
	bool found = false;
	EnterCriticalSection(&s_disconnectLock);
	if (!s_disconnectedSmallIds.empty())
	{
		*outSmallId = s_disconnectedSmallIds.back();
		s_disconnectedSmallIds.pop_back();
		found = true;
	}
	LeaveCriticalSection(&s_disconnectLock);
	return found;
}

void WinsockNetLayer::PushFreeSmallId(BYTE smallId)
{
	EnterCriticalSection(&s_freeSmallIdLock);
	s_freeSmallIds.push_back(smallId);
	LeaveCriticalSection(&s_freeSmallIdLock);
}

bool WinsockNetLayer::PopPendingJoinSmallId(BYTE *outSmallId)
{
	bool found = false;
	EnterCriticalSection(&s_pendingJoinLock);
	if (!s_pendingJoinSmallIds.empty())
	{
		*outSmallId = s_pendingJoinSmallIds.back();
		s_pendingJoinSmallIds.pop_back();
		found = true;
	}
	LeaveCriticalSection(&s_pendingJoinLock);
	return found;
}

bool WinsockNetLayer::IsSmallIdConnected(BYTE smallId)
{
	if ((size_t)smallId >= s_connections.size()) return false;
	return s_connections[smallId].active;
}

void WinsockNetLayer::CloseConnectionBySmallId(BYTE smallId)
{
	EnterCriticalSection(&s_connectionsLock);
	if ((size_t)smallId < s_connections.size() && s_connections[smallId].active && s_connections[smallId].tcpSocket != INVALID_SOCKET)
	{
		s_connections[smallId].active = false;
		closesocket(s_connections[smallId].tcpSocket);
		s_connections[smallId].tcpSocket = INVALID_SOCKET;
		app.DebugPrintf("Win64 LAN: Force-closed TCP connection for smallId=%d\n", smallId);
	}
	LeaveCriticalSection(&s_connectionsLock);

	if ((size_t)smallId < s_connections.size())
	{
		EnterCriticalSection(&s_connections[smallId].sendBufLock);
		s_connections[smallId].sendBufferUsed = 0;
		LeaveCriticalSection(&s_connections[smallId].sendBufLock);
	}

	EnterCriticalSection(&s_earlyDataLock);
	if ((size_t)smallId < s_earlyDataBuffers.size())
		s_earlyDataBuffers[smallId].clear();
	LeaveCriticalSection(&s_earlyDataLock);
}

void WinsockNetLayer::FlushSendBuffers()
{
	if (!s_isHost) return;

	for (size_t i = 0; i < s_connections.size(); i++)
	{
		if (!s_connections[i].active) continue;

		EnterCriticalSection(&s_connections[i].sendBufLock);
		if (s_connections[i].sendBufferUsed > 0 && s_connections[i].tcpSocket != INVALID_SOCKET)
		{
			int totalSent = 0;
			while (totalSent < s_connections[i].sendBufferUsed)
			{
				int sent = send(s_connections[i].tcpSocket,
					(const char *)(s_connections[i].sendBuffer + totalSent),
					s_connections[i].sendBufferUsed - totalSent, 0);
				if (sent == SOCKET_ERROR)
				{
					int err = WSAGetLastError();
					if (err == WSAEWOULDBLOCK)
						break;
					s_connections[i].active = false;
					EnterCriticalSection(&s_disconnectLock);
					s_disconnectedSmallIds.push_back((BYTE)i);
					LeaveCriticalSection(&s_disconnectLock);
					break;
				}
				if (sent == 0) break;
				totalSent += sent;
			}

			int remaining = s_connections[i].sendBufferUsed - totalSent;
			if (remaining > 0)
				memmove(s_connections[i].sendBuffer, s_connections[i].sendBuffer + totalSent, remaining);
			s_connections[i].sendBufferUsed = remaining;
		}
		LeaveCriticalSection(&s_connections[i].sendBufLock);
	}
}

DWORD WINAPI WinsockNetLayer::ClientRecvThreadProc(LPVOID param)
{
	std::vector<BYTE> recvBuf;
	recvBuf.resize(WIN64_NET_RECV_BUFFER_SIZE);

	while (s_active && s_hostConnectionSocket != INVALID_SOCKET)
	{
		BYTE header[4];
		if (!RecvExact(s_hostConnectionSocket, header, 4))
		{
			app.DebugPrintf("Win64 LAN: Disconnected from host (header)\n");
			break;
		}

		int packetSize = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) | (uint32_t)header[3];

		if (packetSize <= 0 || (unsigned int)packetSize > WIN64_NET_MAX_PACKET_SIZE)
		{
			app.DebugPrintf("Win64 LAN: Invalid packet size %d from host (max=%d)\n",
				packetSize,
				(int)WIN64_NET_MAX_PACKET_SIZE);
			break;
		}

		if ((int)recvBuf.size() < packetSize)
		{
			recvBuf.resize(packetSize);
		}

		if (!RecvExact(s_hostConnectionSocket, &recvBuf[0], packetSize))
		{
			app.DebugPrintf("Win64 LAN: Disconnected from host (body)\n");
			break;
		}

		HandleDataReceived(s_hostSmallId, s_localSmallId, &recvBuf[0], packetSize);
	}

	s_connected = false;
	return 0;
}

bool WinsockNetLayer::StartAdvertising(int gamePort, const wchar_t *hostName, unsigned int gameSettings, unsigned int texPackId, unsigned char subTexId, unsigned short netVer)
{
	if (s_advertising) return true;
	if (!s_initialized) return false;

	EnterCriticalSection(&s_advertiseLock);
	memset(&s_advertiseData, 0, sizeof(s_advertiseData));
	s_advertiseData.magic = WIN64_LAN_BROADCAST_MAGIC;
	s_advertiseData.netVersion = netVer;
	s_advertiseData.gamePort = (WORD)gamePort;
	wcsncpy_s(s_advertiseData.hostName, 32, hostName, _TRUNCATE);
	s_advertiseData.playerCount = 1;
	s_advertiseData.maxPlayers = g_ServerMaxPlayers;
	s_advertiseData.gameHostSettings = gameSettings;
	s_advertiseData.texturePackParentId = texPackId;
	s_advertiseData.subTexturePackId = subTexId;
	s_advertiseData.isJoinable = 0;
#ifdef WITH_SERVER_CODE
	s_advertiseData.isDedicatedServer = 1;
#else
	s_advertiseData.isDedicatedServer = 0;
#endif
	s_hostGamePort = gamePort;
	LeaveCriticalSection(&s_advertiseLock);

	s_advertiseSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_advertiseSock == INVALID_SOCKET)
	{
		app.DebugPrintf("Win64 LAN: Failed to create advertise socket: %d\n", WSAGetLastError());
		return false;
	}

	BOOL broadcast = TRUE;
	setsockopt(s_advertiseSock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));

	s_advertising = true;
	s_advertiseThread = CreateThread(NULL, 0, AdvertiseThreadProc, NULL, 0, NULL);

	app.DebugPrintf("Win64 LAN: Started advertising on UDP port %d\n", gamePort);
	return true;
}

void WinsockNetLayer::StopAdvertising()
{
	s_advertising = false;

	if (s_advertiseSock != INVALID_SOCKET)
	{
		closesocket(s_advertiseSock);
		s_advertiseSock = INVALID_SOCKET;
	}

	if (s_advertiseThread != NULL)
	{
		WaitForSingleObject(s_advertiseThread, 2000);
		CloseHandle(s_advertiseThread);
		s_advertiseThread = NULL;
	}
}

void WinsockNetLayer::UpdateAdvertisePlayerCount(BYTE count)
{
	EnterCriticalSection(&s_advertiseLock);
	s_advertiseData.playerCount = count;
	LeaveCriticalSection(&s_advertiseLock);
}

void WinsockNetLayer::UpdateAdvertisePlayerNames(BYTE count, const char playerNames[][XUSER_NAME_SIZE])
{
	EnterCriticalSection(&s_advertiseLock);
	memset(s_advertiseData.playerNames, 0, sizeof(s_advertiseData.playerNames));
	s_advertiseData.playerCount = count;
	for (int i = 0; i < count && i < WIN64_LAN_BROADCAST_PLAYERS; i++)
	{
		memcpy(s_advertiseData.playerNames[i], playerNames[i], XUSER_NAME_SIZE);
	}
	LeaveCriticalSection(&s_advertiseLock);
}

void WinsockNetLayer::UpdateAdvertiseJoinable(bool joinable)
{
	EnterCriticalSection(&s_advertiseLock);
	s_advertiseData.isJoinable = joinable ? 1 : 0;
	LeaveCriticalSection(&s_advertiseLock);
}

void WinsockNetLayer::UpdateAdvertiseGameHostSettings(unsigned int settings)
{
	EnterCriticalSection(&s_advertiseLock);
	s_advertiseData.gameHostSettings = settings;
	LeaveCriticalSection(&s_advertiseLock);
}

DWORD WINAPI WinsockNetLayer::AdvertiseThreadProc(LPVOID param)
{
	struct sockaddr_in broadcastAddr;
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

	while (s_advertising)
	{
		EnterCriticalSection(&s_advertiseLock);
		Win64LANBroadcast data = s_advertiseData;
		int gamePort = s_hostGamePort;
		LeaveCriticalSection(&s_advertiseLock);

		broadcastAddr.sin_port = htons((u_short)gamePort);
		int sent = sendto(s_advertiseSock, (const char *)&data, sizeof(data), 0,
			(struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr));

		if (sent == SOCKET_ERROR && s_advertising)
			app.DebugPrintf("Win64 LAN: Broadcast sendto failed: %d\n", WSAGetLastError());

		if (gamePort != WIN64_LAN_DISCOVERY_PORT)
		{
			broadcastAddr.sin_port = htons(WIN64_LAN_DISCOVERY_PORT);
			sendto(s_advertiseSock, (const char *)&data, sizeof(data), 0,
				(struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr));
		}

		Sleep(1000);
	}

	return 0;
}

bool WinsockNetLayer::StartDiscovery()
{
	if (s_discovering) return true;
	if (!s_initialized) return false;

	s_discoverySock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_discoverySock == INVALID_SOCKET)
	{
		app.DebugPrintf("Win64 LAN: Failed to create discovery socket: %d\n", WSAGetLastError());
		return false;
	}

	BOOL reuseAddr = TRUE;
	setsockopt(s_discoverySock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseAddr, sizeof(reuseAddr));

	struct sockaddr_in bindAddr;
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(WIN64_NET_DEFAULT_PORT);
	bindAddr.sin_addr.s_addr = INADDR_ANY;

	if (::bind(s_discoverySock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
	{
		app.DebugPrintf("Win64 LAN: Discovery bind to port %d failed: %d\n", WIN64_NET_DEFAULT_PORT, WSAGetLastError());
		closesocket(s_discoverySock);
		s_discoverySock = INVALID_SOCKET;
	}
	else
	{
		DWORD timeout = 500;
		setsockopt(s_discoverySock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
	}

	s_discoveryLegacySock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_discoveryLegacySock != INVALID_SOCKET)
	{
		setsockopt(s_discoveryLegacySock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseAddr, sizeof(reuseAddr));

		bindAddr.sin_port = htons(WIN64_LAN_DISCOVERY_PORT);
		if (::bind(s_discoveryLegacySock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
		{
			app.DebugPrintf("Win64 LAN: Legacy discovery bind to port %d failed: %d\n", WIN64_LAN_DISCOVERY_PORT, WSAGetLastError());
			closesocket(s_discoveryLegacySock);
			s_discoveryLegacySock = INVALID_SOCKET;
		}
		else
		{
			DWORD timeout = 500;
			setsockopt(s_discoveryLegacySock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
		}
	}

	if (s_discoverySock == INVALID_SOCKET && s_discoveryLegacySock == INVALID_SOCKET)
		return false;

	s_discovering = true;
	s_discoveryThread = CreateThread(NULL, 0, DiscoveryThreadProc, NULL, 0, NULL);

	app.DebugPrintf("Win64 LAN: Listening for LAN games on UDP ports %d and %d\n", WIN64_NET_DEFAULT_PORT, WIN64_LAN_DISCOVERY_PORT);
	return true;
}

void WinsockNetLayer::StopDiscovery()
{
	s_discovering = false;

	if (s_discoverySock != INVALID_SOCKET)
	{
		closesocket(s_discoverySock);
		s_discoverySock = INVALID_SOCKET;
	}

	if (s_discoveryLegacySock != INVALID_SOCKET)
	{
		closesocket(s_discoveryLegacySock);
		s_discoveryLegacySock = INVALID_SOCKET;
	}

	if (s_discoveryThread != NULL)
	{
		WaitForSingleObject(s_discoveryThread, 2000);
		CloseHandle(s_discoveryThread);
		s_discoveryThread = NULL;
	}

	EnterCriticalSection(&s_discoveryLock);
	s_discoveredSessions.clear();
	LeaveCriticalSection(&s_discoveryLock);
}

std::vector<Win64LANSession> WinsockNetLayer::GetDiscoveredSessions()
{
	std::vector<Win64LANSession> result;
	EnterCriticalSection(&s_discoveryLock);
	result = s_discoveredSessions;
	LeaveCriticalSection(&s_discoveryLock);
	return result;
}

DWORD WINAPI WinsockNetLayer::DiscoveryThreadProc(LPVOID param)
{
	char recvBuf[1024];
	const size_t MAX_DISCOVERED_SESSIONS = 64;

	while (s_discovering)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		int nfds = 0;
		if (s_discoverySock != INVALID_SOCKET)
		{
			FD_SET(s_discoverySock, &readSet);
			if ((int)s_discoverySock > nfds) nfds = (int)s_discoverySock;
		}
		if (s_discoveryLegacySock != INVALID_SOCKET)
		{
			FD_SET(s_discoveryLegacySock, &readSet);
			if ((int)s_discoveryLegacySock > nfds) nfds = (int)s_discoveryLegacySock;
		}

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		int selResult = select(nfds + 1, &readSet, NULL, NULL, &tv);
		if (selResult <= 0)
			continue;

		SOCKET readySocks[2];
		int readyCount = 0;
		if (s_discoverySock != INVALID_SOCKET && FD_ISSET(s_discoverySock, &readSet))
			readySocks[readyCount++] = s_discoverySock;
		if (s_discoveryLegacySock != INVALID_SOCKET && FD_ISSET(s_discoveryLegacySock, &readSet))
			readySocks[readyCount++] = s_discoveryLegacySock;

		for (int si = 0; si < readyCount; si++)
		{
			struct sockaddr_in senderAddr;
			int senderLen = sizeof(senderAddr);

			int recvLen = recvfrom(readySocks[si], recvBuf, sizeof(recvBuf), 0,
				(struct sockaddr *)&senderAddr, &senderLen);

			if (recvLen == SOCKET_ERROR)
				continue;

			if (recvLen < (int)sizeof(Win64LANBroadcast))
				continue;

			Win64LANBroadcast *broadcast = (Win64LANBroadcast *)recvBuf;
			if (broadcast->magic != WIN64_LAN_BROADCAST_MAGIC)
				continue;

			broadcast->hostName[31] = L'\0';

			for (int pn = 0; pn < WIN64_LAN_BROADCAST_PLAYERS; pn++)
				broadcast->playerNames[pn][XUSER_NAME_SIZE - 1] = '\0';

			char senderIP[64];
			inet_ntop(AF_INET, &senderAddr.sin_addr, senderIP, sizeof(senderIP));

			DWORD now = GetTickCount();

			EnterCriticalSection(&s_discoveryLock);

			bool found = false;
			for (size_t i = 0; i < s_discoveredSessions.size(); i++)
			{
				if (strcmp(s_discoveredSessions[i].hostIP, senderIP) == 0 &&
					s_discoveredSessions[i].hostPort == (int)broadcast->gamePort)
				{
					s_discoveredSessions[i].netVersion = broadcast->netVersion;
					wcsncpy_s(s_discoveredSessions[i].hostName, 32, broadcast->hostName, _TRUNCATE);
					s_discoveredSessions[i].playerCount = broadcast->playerCount;
					s_discoveredSessions[i].maxPlayers = broadcast->maxPlayers;
					s_discoveredSessions[i].gameHostSettings = broadcast->gameHostSettings;
					s_discoveredSessions[i].texturePackParentId = broadcast->texturePackParentId;
					s_discoveredSessions[i].subTexturePackId = broadcast->subTexturePackId;
					s_discoveredSessions[i].isJoinable = (broadcast->isJoinable != 0);
					s_discoveredSessions[i].isDedicatedServer = (broadcast->isDedicatedServer != 0);
					s_discoveredSessions[i].lastSeenTick = now;
					memcpy(s_discoveredSessions[i].playerNames, broadcast->playerNames, sizeof(broadcast->playerNames));
					found = true;
					break;
				}
			}

			if (!found)
			{
				if (s_discoveredSessions.size() >= MAX_DISCOVERED_SESSIONS)
				{
					LeaveCriticalSection(&s_discoveryLock);
					continue;
				}

				Win64LANSession session;
				memset(&session, 0, sizeof(session));
				strncpy_s(session.hostIP, sizeof(session.hostIP), senderIP, _TRUNCATE);
				session.hostPort = (int)broadcast->gamePort;
				session.netVersion = broadcast->netVersion;
				wcsncpy_s(session.hostName, 32, broadcast->hostName, _TRUNCATE);
				session.playerCount = broadcast->playerCount;
				session.maxPlayers = broadcast->maxPlayers;
				session.gameHostSettings = broadcast->gameHostSettings;
				session.texturePackParentId = broadcast->texturePackParentId;
				session.subTexturePackId = broadcast->subTexturePackId;
				session.isJoinable = (broadcast->isJoinable != 0);
				session.isDedicatedServer = (broadcast->isDedicatedServer != 0);
				session.lastSeenTick = now;
				memcpy(session.playerNames, broadcast->playerNames, sizeof(broadcast->playerNames));
				s_discoveredSessions.push_back(session);

				app.DebugPrintf("Win64 LAN: Discovered game \"%ls\" at %s:%d\n",
					session.hostName, session.hostIP, session.hostPort);
			}

			for (size_t i = s_discoveredSessions.size(); i > 0; i--)
			{
				if (now - s_discoveredSessions[i - 1].lastSeenTick > 5000)
				{
					app.DebugPrintf("Win64 LAN: Session \"%ls\" at %s timed out\n",
						s_discoveredSessions[i - 1].hostName, s_discoveredSessions[i - 1].hostIP);
					s_discoveredSessions.erase(s_discoveredSessions.begin() + (i - 1));
				}
			}

			LeaveCriticalSection(&s_discoveryLock);
		}
	}

	return 0;
}

#endif
