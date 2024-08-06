#pragma once
#include "SharedMemoryInterface.h"
#include <string_view>
#include <map>
#include <vector>
#include <atomic>

namespace ix {
	class WebSocketServer;
	class WebSocket;
	class ConnectionState;
	struct WebSocketMessage;
	struct SocketTLSOptions;
	using WebSocketMessagePtr = std::unique_ptr<WebSocketMessage>;
}

class LocalServer;

class WebsocketServer
{
public:
	enum { port = 34013 };

	enum class ConnectionType
	{
		LocalHost,
		PrivateInternet,
		PrivateNetwork,
		ProbablyARouter,
		AutomaticPrivateAddress,

		PublicNetwork
	};

	static ConnectionType IsPrivateIp(std::string const& string);

	WebsocketServer();
	~WebsocketServer();

	union IP
	{
		int Read(std::string const&);

		int ipv4[4];
		int ipv6[6];
	};

	void OnGameOpened(SharedMemoryInterface*);
	void OnGameClosed(SharedMemoryInterface*);
	void CloseUnaffiliatedClients();

	bool Parse(std::string_view,  std::shared_ptr<ix::WebSocket> parent = nullptr);

private:
	void OnConnection(std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState);
	void OnMessageCallback(std::weak_ptr<ix::WebSocket> webSocket, const ix::WebSocketMessagePtr& msg);

struct ParseResult;
	ParseResult GetParseResult(std::string_view str);

	std::mutex _mutex;
	SharedMemoryInterface* _interface{};
	std::unique_ptr<LocalServer>			m_localServer;
	std::unique_ptr<ix::WebSocketServer>	m_server;
	std::unique_ptr<ix::SocketTLSOptions>	m_tls;
	std::multimap<std::string, std::weak_ptr<ix::WebSocket>> socketsByProtocol;

	struct ClientConnection
	{
		std::shared_ptr<ix::WebSocket> socket;
		std::weak_ptr<ix::WebSocket> parent;
		bool isGameConnection{};
	};

	std::vector<ClientConnection> _clients;
	std::vector<std::weak_ptr<ix::WebSocket>> _allConnections;
	std::atomic<bool> portClosed{};
};

