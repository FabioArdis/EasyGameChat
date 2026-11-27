#include <easygamechat/easygamechat.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "platform.hpp"
#include "tls_socket.hpp"

namespace egc {
namespace {

constexpr int kConnectTimeoutMs = 5000;
constexpr int kRecvTimeoutMs = 100;
constexpr int kMinSendIntervalMs = 100;
constexpr std::size_t kMaxMessagesPerLoop = 10;

std::vector<std::string> extractJsonObjects(std::string& buffer) {
  std::vector<std::string> objects;
  std::size_t start = std::string::npos;
  std::size_t consumed = 0;
  int depth = 0;
  bool inString = false;
  bool escaped = false;

  for (std::size_t i = 0; i < buffer.size() && objects.size() < kMaxMessagesPerLoop; ++i) {
    char c = buffer[i];

    if (start == std::string::npos) {
      if (c == '{') {
        start = i;
        depth = 1;
      }
      continue;
    }

    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (c == '"') {
      inString = !inString;
      continue;
    }
    if (inString) {
      continue;
    }

    if (c == '{') {
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) {
        objects.push_back(buffer.substr(start, i - start + 1));
        consumed = i + 1;
        start = std::string::npos;
      }
    }
  }

  if (start != std::string::npos) {
    buffer.erase(0, start);
  } else if (consumed > 0) {
    buffer.erase(0, consumed);
  } else {
    buffer.clear();
  }

  return objects;
}

}  // namespace

EasyGameChat::EasyGameChat(const std::string& host, int port, bool useTLS)
    : _host(host),
      _port(port),
      _winsockInit(new WinSockInitializer()),
      _tlsInit(new TLSInitializer()),
      _useTLS(useTLS),
      _lastSendTime(std::chrono::steady_clock::now()) {
  if (_useTLS) {
    _tlsSocket.reset(new TLSSocket());
  }
}

EasyGameChat::~EasyGameChat() {
  disconnect();
}

bool EasyGameChat::sendJson(const std::string& jsonStr) {
  if (_sock.load() < 0) {
    return false;
  }
  if (!isSecureJson(jsonStr)) {
    return false;
  }

  std::string message = jsonStr;
  message += '\n';

  ssize_t sent;
  if (_useTLS && _tlsSocket) {
    sent = _tlsSocket->send(message.data(), message.size());
  } else {
#ifdef _WIN32
    sent = ::send(_sock.load(), message.data(), static_cast<int>(message.size()), 0);
#else
    sent = ::send(_sock.load(), message.data(), message.size(), 0);
#endif
  }

  return sent == static_cast<ssize_t>(message.size());
}

bool EasyGameChat::connect(const std::string& nickname, const std::string& token) {
  if (_running.load()) {
    return false;
  }
  if (!isValidNickname(nickname)) {
    return false;
  }

  _nickname = nickname;

  nlohmann::json hello;
  hello["from"] = "Client";
  hello["text"] = nickname;
  hello["token"] = token;

  bool connected = false;

  if (_useTLS) {
    if (!_tlsSocket) {
      return false;
    }
    connected = _tlsSocket->connect(_host, _port);
    if (connected) {
      _sock.store(_tlsSocket->getSocket());
      if (!sendJson(hello.dump())) {
        _tlsSocket->disconnect();
        _sock.store(-1);
        return false;
      }
    }
  } else {
    int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
      return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(_port));

#ifdef _WIN32
    if (InetPtonA(AF_INET, _host.c_str(), &addr.sin_addr) != 1) {
      detail::closeSocket(sock);
      return false;
    }
#else
    if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) <= 0) {
      detail::closeSocket(sock);
      return false;
    }
#endif

    if (!detail::connectWithTimeout(
            sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr), kConnectTimeoutMs)) {
      detail::closeSocket(sock);
      return false;
    }

    _sock.store(sock);

    if (!sendJson(hello.dump())) {
      detail::closeSocket(sock);
      _sock.store(-1);
      return false;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    struct timeval timeout;
    timeout.tv_sec = kConnectTimeoutMs / 1000;
    timeout.tv_usec = (kConnectTimeoutMs % 1000) * 1000;

    if (select(sock + 1, &readSet, nullptr, nullptr, &timeout) <= 0) {
      detail::closeSocket(sock);
      _sock.store(-1);
      return false;
    }

    std::vector<char> response(1024);
    ssize_t received = ::recv(sock, response.data(), static_cast<int>(response.size() - 1), 0);
    if (received <= 0) {
      detail::closeSocket(sock);
      _sock.store(-1);
      return false;
    }
    response[static_cast<std::size_t>(received)] = '\0';

    try {
      auto parsed = nlohmann::json::parse(std::string(response.data(), static_cast<std::size_t>(received)));
      if (parsed.contains("status") && parsed["status"] != "success") {
        detail::closeSocket(sock);
        _sock.store(-1);
        return false;
      }
    } catch (const nlohmann::json::exception&) {
      detail::closeSocket(sock);
      _sock.store(-1);
      return false;
    }

    if (!detail::setNonBlocking(sock)) {
      detail::closeSocket(sock);
      _sock.store(-1);
      return false;
    }

    connected = true;
  }

  if (!connected) {
    return false;
  }

  _shouldStop.store(false);
  _running.store(true);
  _recvThread = std::thread(&EasyGameChat::recvLoop, this);
  return true;
}

void EasyGameChat::recvLoop() {
  std::vector<char> buffer(MAX_BUFFER_SIZE);
  std::string leftover;
  leftover.reserve(MAX_BUFFER_SIZE);

  while (_running.load() && !_shouldStop.load()) {
    if (_sock.load() < 0) {
      break;
    }

    ssize_t received;
    if (_useTLS && _tlsSocket) {
      received = _tlsSocket->recv(buffer.data(), buffer.size());
    } else {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(_sock.load(), &readSet);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = kRecvTimeoutMs * 1000;

      int selectResult = select(_sock.load() + 1, &readSet, nullptr, nullptr, &timeout);
      if (selectResult < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEINTR) {
          continue;
        }
#else
        if (errno == EINTR) {
          continue;
        }
#endif
        break;
      }
      if (selectResult == 0) {
        continue;
      }

      received = ::recv(_sock.load(), buffer.data(), static_cast<int>(buffer.size()), 0);
    }

    if (received > 0) {
      if (leftover.size() + static_cast<std::size_t>(received) > MAX_BUFFER_SIZE) {
        leftover.clear();
        continue;
      }

      leftover.append(buffer.data(), static_cast<std::size_t>(received));

      while (true) {
        std::vector<std::string> objects = extractJsonObjects(leftover);
        if (objects.empty()) {
          break;
        }

        for (const std::string& object : objects) {
          try {
            auto message = nlohmann::json::parse(object);

            if (message.contains("status")) {
              continue;
            }

            if (message.contains("from") && message.contains("text")) {
              std::string from = message["from"].get<std::string>();
              std::string text = message["text"].get<std::string>();

              std::lock_guard<std::mutex> lock(_callbackMutex);
              if (_callback) {
                _callback(from, text);
              }
            }
          } catch (const std::exception&) {
            continue;
          }
        }
      }
    } else if (received == 0) {
      break;
    } else {
#ifdef _WIN32
      int error = WSAGetLastError();
      if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
        _shouldStop.store(true);
        break;
      }
#else
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        _shouldStop.store(true);
        break;
      }
#endif
    }
  }
}

bool EasyGameChat::sendMessage(const std::string& text) {
  if (_sock.load() < 0 || !_running.load()) {
    return false;
  }
  if (!isValidMessage(text)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(_sendMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastSendTime);
    if (elapsed.count() < kMinSendIntervalMs) {
      return false;
    }
    _lastSendTime = now;
  }

  nlohmann::json message;
  message["from"] = _nickname;
  message["text"] = text;

  return sendJson(message.dump());
}

void EasyGameChat::setMessageCallback(MessageCallback cb) {
  std::lock_guard<std::mutex> lock(_callbackMutex);
  _callback = std::move(cb);
}

void EasyGameChat::disconnect() {
  if (_running.exchange(false)) {
    _shouldStop.store(true);

    if (_recvThread.joinable()) {
      _recvThread.join();
    }

    if (_useTLS && _tlsSocket) {
      _tlsSocket->disconnect();
    } else {
      int sock = _sock.exchange(-1);
      if (sock >= 0) {
        detail::closeSocket(sock);
      }
    }

    _sock.store(-1);
  }
}

ConnectionInfo EasyGameChat::getConnectionInfo() const {
  ConnectionInfo info;
  info.connected = _running.load();
  info.host = _host;
  info.port = _port;
  info.nickname = _nickname;
  info.tls_enabled = _useTLS;

  if (_useTLS && _tlsSocket && _running.load()) {
    try {
      info = _tlsSocket->getConnectionInfo(info);
    } catch (const std::exception& e) {
      info.tls_error = e.what();
    }
  }

  return info;
}

bool EasyGameChat::shouldStop() const {
  return _shouldStop.load();
}

}  // namespace egc
