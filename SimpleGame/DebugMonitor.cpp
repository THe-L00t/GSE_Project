#include "stdafx.h"
#include "DebugMonitor.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace
{
	void SendAll(SOCKET s, const std::string& data)
	{
		size_t sent = 0;
		while (sent < data.size())
		{
			int n = send(s, data.c_str() + sent, (int)(data.size() - sent), 0);
			if (n <= 0) return;
			sent += (size_t)n;
		}
	}

	void SendResponse(SOCKET s, const char* status, const char* contentType, const std::string& body)
	{
		char header[256];
		sprintf_s(header, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
				  status, contentType, (unsigned)body.size());
		SendAll(s, header);
		SendAll(s, body);
	}

	bool LoadText(const char* path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) return false;
		std::ostringstream ss;
		ss << file.rdbuf();
		out = ss.str();
		return true;
	}
}

DebugMonitor::~DebugMonitor()
{
	Stop();
}

bool DebugMonitor::Start(int listenPort)
{
	if (IsRunning()) return true;

	if (!wsaReady)
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
		wsaReady = true;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) return false;

	// Loopback only: the monitor is for the person at this machine.
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)listenPort);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	u_long nonBlocking = 1;
	if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(s, 8) != 0 || ioctlsocket(s, FIONBIO, &nonBlocking) != 0)
	{
		closesocket(s);
		return false;
	}

	listener = (uintptr_t)s;
	port = listenPort;
	return true;
}

void DebugMonitor::Stop()
{
	if (IsRunning())
	{
		closesocket((SOCKET)listener);
		listener = kNoSocket;
	}
	if (wsaReady)
	{
		WSACleanup();
		wsaReady = false;
	}
}

void DebugMonitor::Poll(const std::function<std::string()>& stateJson)
{
	if (!IsRunning()) return;

	for (;;)
	{
		SOCKET client = accept((SOCKET)listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) return;
		Serve((uintptr_t)client, stateJson);
		closesocket(client);
	}
}

void DebugMonitor::Serve(uintptr_t clientHandle, const std::function<std::string()>& stateJson)
{
	SOCKET client = (SOCKET)clientHandle;

	// The accepted socket inherits non-blocking; a short wait lets the request line arrive.
	u_long blocking = 0;
	ioctlsocket(client, FIONBIO, &blocking);
	DWORD timeoutMs = 50;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

	char request[2048];
	int n = recv(client, request, sizeof(request) - 1, 0);
	if (n <= 0) return;
	request[n] = '\0';

	char method[8] = "";
	char path[256] = "";
	if (sscanf_s(request, "%7s %255s", method, (unsigned)sizeof(method), path, (unsigned)sizeof(path)) != 2) return;

	if (strcmp(method, "GET") != 0)
	{
		SendResponse(client, "405 Method Not Allowed", "text/plain; charset=utf-8", "GET only");
		return;
	}

	if (strcmp(path, "/state") == 0)
	{
		SendResponse(client, "200 OK", "application/json; charset=utf-8", stateJson());
		return;
	}

	if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
	{
		// Read on every request so the page can be edited while the game runs.
		std::string page;
		if (LoadText("Monitor.html", page))
			SendResponse(client, "200 OK", "text/html; charset=utf-8", page);
		else
			SendResponse(client, "404 Not Found", "text/plain; charset=utf-8", "Monitor.html not found in the working directory");
		return;
	}

	SendResponse(client, "404 Not Found", "text/plain; charset=utf-8", "not found");
}
