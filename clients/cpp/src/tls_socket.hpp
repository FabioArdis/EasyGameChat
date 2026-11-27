#ifndef EASYGAMECHAT_TLS_SOCKET_HPP
#define EASYGAMECHAT_TLS_SOCKET_HPP

#include <easygamechat/easygamechat.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "platform.hpp"

#ifdef _WIN32
#include <schannel.h>
#include <security.h>
#include <sspi.h>
#else
#include <openssl/ssl.h>
#endif

namespace egc {

class TLSInitializer {
 public:
  TLSInitializer();
  ~TLSInitializer();

  TLSInitializer(const TLSInitializer&) = delete;
  TLSInitializer& operator=(const TLSInitializer&) = delete;
};

class WinSockInitializer {
 public:
  WinSockInitializer();
  ~WinSockInitializer();

  WinSockInitializer(const WinSockInitializer&) = delete;
  WinSockInitializer& operator=(const WinSockInitializer&) = delete;
};

class TLSSocket {
 public:
  explicit TLSSocket(const TLSConfig& config);
  ~TLSSocket();

  TLSSocket(const TLSSocket&) = delete;
  TLSSocket& operator=(const TLSSocket&) = delete;

  bool connect(const std::string& host, int port);
  ssize_t send(const void* data, std::size_t len);
  ssize_t recv(void* data, std::size_t len);
  void disconnect();
  bool isConnected() const;
  int getSocket() const;
  ConnectionInfo getConnectionInfo(ConnectionInfo info) const;

 private:
  bool performTLSHandshake(const std::string& host);

#ifdef _WIN32
  bool performWindowsTLSHandshake(const std::string& host);
  bool completeWindowsHandshake();
  ssize_t sendWindows(const void* data, std::size_t len);
  ssize_t recvWindows(void* data, std::size_t len);
  ConnectionInfo getWindowsConnectionInfo(ConnectionInfo info) const;

  CtxtHandle context;
  CredHandle credentials;
  bool contextInitialized = false;
  bool credentialsInitialized = false;
  std::vector<char> readBuffer;
  std::vector<char> writeBuffer;
#else
  bool performOpenSSLHandshake(const std::string& host);
  ConnectionInfo getOpenSSLConnectionInfo(ConnectionInfo info) const;

  SSL_CTX* ctx = nullptr;
  SSL* ssl = nullptr;
#endif

  bool verifyCertificate_;
  int socket;
  bool connected = false;
};

}  // namespace egc

#endif
