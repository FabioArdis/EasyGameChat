#ifndef EASYGAMECHAT_PLATFORM_HPP
#define EASYGAMECHAT_PLATFORM_HPP

#include <cstddef>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
typedef int socklen_t;
typedef SSIZE_T ssize_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

namespace egc {
namespace detail {

inline void sleepMs(int ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts{};
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, nullptr);
#endif
}

inline bool setNonBlocking(int sock) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    return false;
  }
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

inline bool setBlocking(int sock) {
#ifdef _WIN32
  u_long mode = 0;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    return false;
  }
  return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) != -1;
#endif
}

inline void closeSocket(int sock) {
#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif
}

inline int lastSocketError() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline bool connectWithTimeout(int sock, const sockaddr* addr, socklen_t addrlen, int timeoutMs) {
  if (!setNonBlocking(sock)) {
    return false;
  }

  int result = ::connect(sock, addr, addrlen);
  if (result == 0) {
    return setBlocking(sock);
  }

#ifdef _WIN32
  if (WSAGetLastError() != WSAEWOULDBLOCK) {
    return false;
  }
#else
  if (errno != EINPROGRESS) {
    return false;
  }
#endif

  fd_set writeSet;
  FD_ZERO(&writeSet);
  FD_SET(sock, &writeSet);

  struct timeval timeout;
  timeout.tv_sec = timeoutMs / 1000;
  timeout.tv_usec = (timeoutMs % 1000) * 1000;

  int selectResult = select(sock + 1, nullptr, &writeSet, nullptr, &timeout);
  if (selectResult <= 0) {
    return false;
  }

  int error = 0;
  socklen_t errorLen = sizeof(error);
  if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &errorLen) < 0) {
    return false;
  }
  if (error != 0) {
    return false;
  }

  return setBlocking(sock);
}

}  // namespace detail
}  // namespace egc

#endif
