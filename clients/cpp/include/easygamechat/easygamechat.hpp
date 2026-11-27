#ifndef EASYGAMECHAT_HPP
#define EASYGAMECHAT_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace egc {

constexpr std::size_t MAX_NICKNAME_LENGTH = 32;
constexpr std::size_t MAX_MESSAGE_LENGTH = 512;
constexpr std::size_t MAX_BUFFER_SIZE = 4096;

struct ConnectionInfo {
  bool connected = false;
  std::string host;
  int port = 0;
  std::string nickname;
  bool tls_enabled = false;
  std::string tls_cipher;
  std::string tls_version;
  int tls_bits = 0;
  std::string server_cert_cn;
  bool server_cert_valid = false;
  std::string tls_error;
};

bool isValidNickname(const std::string& nickname);
bool isValidMessage(const std::string& message);
bool isSecureJson(const std::string& str);

class TLSSocket;
class TLSInitializer;
class WinSockInitializer;

class EasyGameChat {
 public:
  using MessageCallback = std::function<void(const std::string& from, const std::string& text)>;

  EasyGameChat(const std::string& host, int port, bool useTLS = true);
  ~EasyGameChat();

  EasyGameChat(const EasyGameChat&) = delete;
  EasyGameChat& operator=(const EasyGameChat&) = delete;

  bool connect(const std::string& nickname, const std::string& token);
  bool sendMessage(const std::string& text);
  void setMessageCallback(MessageCallback cb);
  void disconnect();
  ConnectionInfo getConnectionInfo() const;
  bool shouldStop() const;

 private:
  void recvLoop();
  bool sendJson(const std::string& jsonStr);

  std::string _host;
  int _port;
  std::atomic<int> _sock{-1};
  std::string _nickname;
  std::thread _recvThread;
  std::atomic<bool> _running{false};
  std::atomic<bool> _shouldStop{false};
  MessageCallback _callback;
  std::mutex _callbackMutex;
  std::unique_ptr<WinSockInitializer> _winsockInit;
  std::unique_ptr<TLSInitializer> _tlsInit;
  std::unique_ptr<TLSSocket> _tlsSocket;
  bool _useTLS;
  std::chrono::steady_clock::time_point _lastSendTime;
  std::mutex _sendMutex;
};

}  // namespace egc

#endif
