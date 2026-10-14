#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Read-only browser view of the game state: a tiny HTTP server polled from the main loop.
// "/" serves Monitor.html, "/state" serves the JSON the callback builds.
class DebugMonitor
{
public:
	~DebugMonitor();

	bool Start(int port);
	void Stop();
	bool IsRunning() const { return listener != kNoSocket; }
	int  Port() const { return port; }

	// Never blocks on the listener; answers every request that is waiting.
	void Poll(const std::function<std::string()>& stateJson);

private:
	static const uintptr_t kNoSocket = ~(uintptr_t)0;

	void Serve(uintptr_t client, const std::function<std::string()>& stateJson);

	uintptr_t listener = kNoSocket;
	int       port = 0;
	bool      wsaReady = false;
};
