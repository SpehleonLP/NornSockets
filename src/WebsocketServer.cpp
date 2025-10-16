#include "WebsocketServer.h"
#include "Support.h"
#include "localserver.h"
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXUserAgent.h>
#include <functional>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <iostream>

extern bool g_Verbose;

void OnFatalError();

WebsocketServer::WebsocketServer()
{
	m_localServer.reset(new LocalServer);
	m_server.reset(new ix::WebSocketServer(port));
	m_tls.reset(new ix::SocketTLSOptions());
	m_tls->tls = true;

	if (!m_tls->isValid())
	{
		fprintf(stderr, "TLS configuration is not valid: %s\n", m_tls->getErrorMsg().c_str());
		m_tls.reset();
	}

	m_server->setOnConnectionCallback(std::bind(&WebsocketServer::OnConnection, this, std::placeholders::_1, std::placeholders::_2));

	auto res = m_server->listen();
	if (!res.first)
	{
		// Error handling
		fprintf(stderr, "Server Creation Error: %s local networking will not be possible.\n", res.second.c_str());
		m_server.reset();
		OnFatalError();
		return;
	}

	// Per message deflate connection is enabled by default. It can be disabled
	// which might be helpful when running on low power devices such as a Rasbery Pi
	m_server->disablePerMessageDeflate();

	// Run the server in the background. Server can be stoped by calling server.stop()
	m_server->start();

	auto url = m_server->getHost();
	fprintf(stdout, "Opened websocket server %s:%d\n", url.c_str(), m_server->getPort());
}

WebsocketServer::~WebsocketServer()
{
	_clients.clear();

	if (m_server)
		m_server->stop();
}

void WebsocketServer::OnGameOpened(SharedMemoryInterface* _interface)
{
	m_localServer->OnGameOpened(_interface);

	std::lock_guard lock(_mutex);
	assert(this->_interface == nullptr);
	this->_interface = _interface;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s %s %d.%d %s", "OnGameOpened", _interface->_engine.c_str(), _interface->versionMajor, _interface->versionMinor, _interface->_name.c_str());
	std::string message = buffer;

	fprintf(stderr, "%s\n", buffer);

	if(_interface->_workingDirectory.empty() == false)
	{
		fprintf(stderr, "%s\n", _interface->_workingDirectory.string().c_str());
	}

	for (auto& item : _allConnections)
	{
		auto agent = item.lock();

		if (agent)
		{
			agent->sendUtf8Text(message);
		}
	}
}


void WebsocketServer::OnGameClosed(SharedMemoryInterface* _interface)
{
	m_localServer->OnGameClosed(_interface);

	std::lock_guard lock(_mutex);
	assert(this->_interface == _interface);
	this->_interface = nullptr;

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s %s %d.%d %s", "OnGameClosed", _interface->_engine.c_str(), _interface->versionMajor, _interface->versionMinor, _interface->_name.c_str());
	std::string message = buffer;

	fprintf(stderr, "%s\n", buffer);

	for (auto & item : _allConnections)
	{
		auto agent = item.lock();

		if (agent)
		{
			agent->sendUtf8Text(message);
		}
	}


	for(auto i = 0u; i < _clients.size(); ++i)
	{
		if(_clients[i].isGameConnection)
		{
			_clients.erase(_clients.begin()+i);
			--i;
		}
	}
}

void WebsocketServer::CloseUnaffiliatedClients()
{
	if(portClosed.exchange(false) == false)
		return;

	std::lock_guard lock(_mutex);

	for(auto i = 0u; i < _clients.size(); ++i)
	{
		if(_clients[i].parent.use_count() == 0)
		{
			_clients.erase(_clients.begin()+i);
			--i;
		}
	}

	for (auto ptr = socketsByProtocol.begin(); ptr != socketsByProtocol.end(); )
	{
		auto next = ptr; ++next;
		auto agent = ptr->second.lock();

		if(ptr->second.use_count() == 0)
		{
			socketsByProtocol.erase(ptr);
		}

		ptr = next;
	}

	for (auto i = 0u; i < _allConnections.size(); ++i)
	{
		if (_allConnections[i].use_count() == 0)
		{
			_allConnections.erase(_allConnections.begin()+i);
			--i;
		}
	}
}

void WebsocketServer::OnConnection(std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState)
{
	auto agent = webSocket.lock();
	std::string remote_ip = connectionState->getRemoteIp();

	if (!agent) return;

	if (m_tls != nullptr)
		agent->setTLSOptions(*m_tls);

	if (!m_tls->isValid())
	{
		fprintf(stderr, "TLS configuration is not valid: %s", m_tls->getErrorMsg().c_str());
	}

	if (IsPrivateIp(remote_ip) == ConnectionType::PublicNetwork)
	{
		fprintf(stderr, "Connection from \"%s\", not allowed: clients must be in private IP range. (because client traffic is unecrypted).", connectionState->getRemoteIp().c_str());
		agent->close();
		return;
	}

	agent->setOnMessageCallback(std::bind(&WebsocketServer::OnMessageCallback, this, webSocket, std::placeholders::_1));
}

void WebsocketServer::OnMessageCallback(std::weak_ptr<ix::WebSocket> webSocket, const ix::WebSocketMessagePtr& msg)
{
	if (msg->type == ix::WebSocketMessageType::Open)
	{
		auto agent = webSocket.lock();
		
		if(agent != nullptr)
		{
			agent->disablePerMessageDeflate();

			std::lock_guard lock(_mutex);
			
			// Get the client's requested protocols from the headers
			std::string requestedProtocols = msg->openInfo.headers["sec-websocket-protocol"];

			if(g_Verbose) fprintf(stderr, "Client requested protocols: %s\n", requestedProtocols.c_str());

			// Parse comma-separated protocol list
			// Note: The handshake is already complete - you can't change it now
			// You can only use this info to route the connection appropriately
			_allConnections.push_back(webSocket);

			// Store by protocol for routing
			if (!requestedProtocols.empty())
			{
			  // Parse the comma-separated list
			  size_t start = 0;
			  while (start < requestedProtocols.length())
			  {
				  // Skip whitespace
				  while (start < requestedProtocols.length() &&
						 std::isspace(requestedProtocols[start])) start++;

				  size_t end = requestedProtocols.find(',', start);
				  if (end == std::string::npos) end = requestedProtocols.length();

				  std::string protocol = requestedProtocols.substr(start, end - start);

				  // Trim trailing whitespace
				  while (!protocol.empty() && std::isspace(protocol.back()))
					  protocol.pop_back();

				  if (!protocol.empty())
				  {
					  if(g_Verbose) fprintf(stderr, "Registered protocol: %s\n", protocol.c_str());
					  socketsByProtocol.insert({protocol, webSocket});
				  }

				  start = end + 1;
			  }
			}

			if (_interface)
			{
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "%s %s %d.%d %s", "OnGameOpened", _interface->_engine.c_str(), _interface->versionMajor, _interface->versionMinor, _interface->_name.c_str());
				agent->sendUtf8Text(buffer);
			}
		}

		return;
	}

	if (msg->type == ix::WebSocketMessageType::Ping
	|| msg->type == ix::WebSocketMessageType::Pong)
		return;

	if (msg->type == ix::WebSocketMessageType::Close)
	{
		portClosed = true;
		fprintf(stderr, "WebSocketClosed (%d): %s", msg->closeInfo.code, msg->closeInfo.reason.data());
		return;
	}

	if (msg->type == ix::WebSocketMessageType::Error)
	{
		fprintf(stderr, "WebSocketError (%d): %s", msg->errorInfo.retries, msg->errorInfo.reason.data());
		return;
	}

	SharedMemoryInterface::Response result;

	if (msg->type == ix::WebSocketMessageType::Message)
	{
		if (msg->binary)
		{
			auto agent = webSocket.lock();

			if(msg->str.size() < 5)
			{
				agent->send("ERROR: binary mode message improperly formatted.");
			}
			else
			{
				uint32_t code{};
				uint32_t byteLength{};
				std::string_view c_str = msg->str.data()+4;
				std::string_view binaryBuffer{};

				memcpy(&code, msg->str.data(), 4);

				if((c_str.data() + c_str.size())+5 < msg->str.data() + msg->str.size())
				{
					memcpy(&byteLength, c_str.data()+c_str.size(), 4);
					binaryBuffer = std::string_view(c_str.data()+c_str.size()+4, msg->str.data() + msg->str.size());

					if(binaryBuffer.size() != byteLength)
					{
						agent->send("ERROR: binary mode message improperly formatted (byte length does not match binary buffer size).");
						return;
					}
				}

				if(code == LocalServer::OOPE)
				{
					Parse(c_str, agent);
				}
				else
				{
					result = m_localServer->ProcessMessage(code, c_str, binaryBuffer);
				}
			}
		}
		else
		{
			try
			{
				result = _interface->send(msg->str);
			}
			catch (std::exception& e)
			{
				auto agent = webSocket.lock();
				agent->close(ix::WebSocketCloseConstants::kNormalClosureCode, e.what());
				throw;
			}
		}

		if(result.text.size())
		{
			auto agent = webSocket.lock();

			if(result.isError)
				agent->close(ix::WebSocketCloseConstants::kProtocolErrorCode, result.text);
			else if(result.isBinary)
				agent->sendBinary(result.text);
			else
				agent->sendUtf8Text(result.text);
		}
	}
}

struct WebsocketServer::ParseResult
{
	std::string url;
	int port{34013};
	std::string message;
	std::string protocol;
	bool noMatch{true};
};

// this function looks for this stuff:
// ws://url:port[protocol]
// wss://url:port[protocol]
// ws://url:port[protocol]:message
// wss://url:port[protocol]:message
// ws[protocol]:message
WebsocketServer::ParseResult WebsocketServer::GetParseResult(std::string_view str)
{
	ParseResult result;
	result.noMatch = true;

	// Helper lambda to find and extract a substring between two delimiters
	auto extract = [](std::string_view s, char start, char end) -> std::string_view {
		auto startPos = s.find(start);
		if (startPos == std::string_view::npos) return {};
		auto endPos = s.find(end, startPos + 1);
		if (endPos == std::string_view::npos) return {};
		return s.substr(startPos + 1, endPos - startPos - 1);
	};

	// Extract sub-protocol from brackets and advance past it
	auto extractSubProtocol = [&extract](std::string_view& s) -> std::string_view {
		auto subProto = extract(s, '[', ']');
		if (!subProto.empty()) {
			auto endBracket = s.find(']');
			if (endBracket != std::string_view::npos) {
				s.remove_prefix(endBracket + 1);
			}
		}
		return subProto;
	};

	// Extract message after colon
	auto extractMessage = [](std::string_view& s) -> std::string_view {
		if (!s.empty() && s.front() == ':') {
			s.remove_prefix(1);
			return s;
		}
		return {};
	};

	// Check for ws:// or wss:// prefix
	if (str.starts_with("ws://") || str.starts_with("wss://")) {
		result.protocol = str.substr(0, str.find("://"));
		std::string_view remaining = str.substr(result.protocol.length() + 3);

		// Extract URL and port
		auto colonPos = remaining.find(':');
		if (colonPos != std::string_view::npos) {
			result.url = remaining.substr(0, colonPos);
			remaining.remove_prefix(colonPos + 1);

			// Extract port
			auto portEnd = remaining.find_first_not_of("0123456789");
			if (portEnd != std::string_view::npos) {
				result.port = std::stoi(std::string(remaining.substr(0, portEnd)));
				remaining.remove_prefix(portEnd);
			} else {
				result.port = std::stoi(std::string(remaining));
				remaining = {};
			}
		}

		// Extract sub-protocol and message
		result.protocol = extractSubProtocol(remaining);
		result.message = extractMessage(remaining);

		result.noMatch = false;
	}
	// Check for ws[protocol]:message format
	else if (str.starts_with("ws")) {
		result.protocol = "ws";
		std::string_view remaining = str.substr(2);
		
		// Extract sub-protocol and message
		result.protocol = extractSubProtocol(remaining);
		result.message = extractMessage(remaining);

		result.noMatch = false;
	}

	return result;
}


bool WebsocketServer::Parse(std::string_view str, std::shared_ptr<ix::WebSocket> parent)
{
	auto parse = GetParseResult(str);

	if(parse.noMatch || parse.protocol.empty())
		return false;
		
	if(g_Verbose)
	{
		std::cout << "original message: " << str << "\n";
		std::cout << "parsed message: {"
			<< "\n\turl     :" << parse.url << ','
			<< "\n\tport    :" << std::to_string(parse.port) << ','
			<< "\n\tmessage :" << parse.message << ','
			<< "\n\tprotocol:" << parse.protocol << ','
			<< "\n}" << std::endl;
	}	

	std::lock_guard lock(_mutex);

// rebuild it just so we're extra sure that it's right.
	std::string _url = ((std::string("wss://") += parse.url) += ":") += std::to_string(port);
	
	if(g_Verbose)
	{
		std::cout << "rebuilt url:" << _url << std::endl;
	}	
	
	if(parse.url.size())
	{
		auto range = socketsByProtocol.equal_range(parse.protocol);

		std::shared_ptr<ix::WebSocket> match;

		for (auto it = range.first; it != range.second; ++it)
		{
			auto agent = it->second.lock();

			if(agent && agent->getUrl() == _url)
			{
				match = agent;
				break;
			}
		}

		if(match == nullptr)
		{
			if(g_Verbose)
			{
				std::cout << "unable to find attached agent matching protocol:" << parse.protocol << std::endl;
			}	
	
			match = std::make_shared<ix::WebSocket>();
			match->setOnMessageCallback(std::bind(&WebsocketServer::OnMessageCallback, this, std::weak_ptr(match), std::placeholders::_1));
			match->addSubProtocol(parse.protocol);
			match->setUrl(_url);
			match->start();
				
			_clients.push_back({
				.socket = match,
				.parent = std::weak_ptr(parent),
				.isGameConnection = (parent == nullptr)
			});

			if(g_Verbose)
			{
				std::cout << "attempted to add new handler for protocol." << std::endl;
			}	
	
			socketsByProtocol.insert({parse.protocol, std::weak_ptr(match)});
		}
		else
		{
			for(auto & p : match->getSubProtocols())
			{
				if(p == parse.protocol)
					goto have_protocol;
			}

			if(g_Verbose)
			{
				std::cout << "attempted to add new handler for protocol." << std::endl;
			}
				
			match->addSubProtocol(parse.protocol);
			socketsByProtocol.insert({parse.protocol, std::weak_ptr(match)});

		have_protocol:
			(void)0;
		}
	}

	if(parse.message.size())
	{
		auto range = socketsByProtocol.equal_range(parse.protocol);

		if(range.first == range.second && g_Verbose)
		{
			fprintf(stderr, "unable to find agent matching protocol: %.*s\nknown protocols: {", int(parse.protocol.size()), parse.protocol.data());
			
			for(auto & item : socketsByProtocol)
			{
				fprintf(stderr, "\t'%s',\n", item.first.c_str());
			}
			
			fprintf(stderr, "}\n");
		}


		for (auto it = range.first; it != range.second; ++it)
		{
			auto agent = it->second.lock();

			if(!agent)
			{
				fprintf(stderr, "agent is null but wasn't removed?");
				continue;
			}
			
			if((parse.url.empty() || _url == agent->getUrl()))
			{
				if(g_Verbose)
				{
					std::cout << "sent message." << std::endl;
				}
			
				agent->sendUtf8Text(parse.message);
			}
			
			else if(g_Verbose)
			{
				std::cout << "agent doesn't match. url (" << agent->getUrl() << ")" << std::endl;
			}
		}
	}

	return true;
}

int WebsocketServer::IP::Read(std::string const& str)
{
#ifdef _WIN32
	if (4 == sscanf_s(str.data(), "%i.%i.%i.%i", &ipv4[0], &ipv4[0], &ipv4[0], &ipv4[0])
#else
	if (4 == sscanf(str.data(), "%i.%i.%i.%i", &ipv4[0], &ipv4[0], &ipv4[0], &ipv4[0])
#endif
		&& 0 <= ipv4[0] && ipv4[0] < 256
		&& 0 <= ipv4[1] && ipv4[1] < 256
		&& 0 <= ipv4[2] && ipv4[2] < 256
		&& 0 <= ipv4[3] && ipv4[3] < 256)
		return 4;

	return -1;

}

WebsocketServer::ConnectionType WebsocketServer::IsPrivateIp(const std::string& ip_string)
{
	if (ip_string == "127.0.0.1" || ip_string == "[::1]" || ip_string == "localhost") {
		return ConnectionType::LocalHost;
	}

	struct sockaddr_in sa;
	struct sockaddr_in6 sa6;

	if (inet_pton(AF_INET, ip_string.c_str(), &(sa.sin_addr)) == 1) {
		// Check IPv4 private addresses
		unsigned char first_octet = ip_string[0];
		unsigned char second_octet = ip_string[1];
		if (first_octet == 10) {
			return ConnectionType::PrivateInternet;
		}
		if (first_octet == 172 && (second_octet >= 16 && second_octet <= 31)) {
			return ConnectionType::PrivateNetwork;
		}
		if (first_octet == 192 && second_octet == 168) {
			return ConnectionType::ProbablyARouter;
		}
		if (first_octet == 169 && second_octet == 254) {
			return ConnectionType::AutomaticPrivateAddress;
		}
	}
	else if (inet_pton(AF_INET6, ip_string.c_str(), &(sa6.sin6_addr)) == 1) {

		if (ip_string.find("fe80:") == 0
			|| ip_string.find("ff00:") == 0) {
			return ConnectionType::ProbablyARouter;  // Link-local addresses
		}

		// Check IPv6 address scopes
		char first_char = ip_string[0];
		char second_char = ip_string[1];
		if (first_char == 'f' && (second_char == 'c' || second_char == 'd')) {
			return ConnectionType::PrivateInternet;
		}
		// Other IPv6 checks here as necessary
	}

	return ConnectionType::PublicNetwork;
}
