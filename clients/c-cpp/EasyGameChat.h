/**
 * @file EasyGameChat.h
 * @brief EasyGameChat - A secure, cross-platform C/C++ library for real-time game chat
 * 
 * EasyGameChat provides a simple yet secure interface for implementing chat functionality
 * in games and applications. It supports both C and C++ APIs with built-in security
 * features including input validation, rate limiting, and protection against common
 * network attacks.
 * 
 * Features:
 * - Cross-platform support (Windows/Linux/macOS)
 * - Thread-safe operations
 * - Input validation and sanitization
 * - Rate limiting to prevent spam
 * - JSON-based protocol
 * - Both blocking and non-blocking socket operations
 * - Memory-safe C API wrapper
 * 
 * @author Fabio Ardis
 * @version 0.1.0
 * @date 2025-06-02
 */

#ifndef EASYGAMECHAT_H
#define EASYGAMECHAT_H

#include <nlohmann/json.hpp>

// Platform-specific includes and definitions
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <WinSock2.h>
  #include <WS2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #include <Windows.h>
  typedef int socklen_t;
  typedef SSIZE_T ssize_t;
  #include <schannel.h>
  #include <security.h>
  #include <sspi.h>
  #pragma comment(lib, "secur32.lib")
  #pragma comment(lib, "crypt32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <string.h>
  #include <time.h>
  #include <sys/select.h>
  #include <openssl/ssl.s>
  #include <openssl/err.h>
  #include <openssl/bio.h>
  #include <openssl/x509v3.h>
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- C API ---

/**
 * @brief Callback function type for receiving chat messages
 * 
 * This callback is invoked whenever a new message is received from the server.
 * The callback should be thread-safe as it may be called from the internal
 * receive thread.
 * 
 * @param from The nickname of the message sender (null-terminated string)
 * @param text The message content (null-terminated string)
 * @param user_data User-provided data pointer passed to egc_set_message_callback
 * 
 * @note The strings pointed to by 'from' and 'text' are only valid during
 *       the callback execution. Copy them if you need to store them.
 * @warning This callback may be called from a different thread than the main thread.
 */
typedef void (*egc_message_callback)(const char* from, const char* text, void* user_data);

/**
 * @brief Opaque client handle for C API
 * 
 * This structure represents a chat client connection. It should be treated
 * as an opaque handle and only accessed through the provided API functions.
 */
typedef struct egc_client_t egc_client_t;

/**
 * @brief Create a new chat client instance
 * 
 * Creates a new EasyGameChat client configured to connect to the specified
 * host and port. This function does not establish a connection; use egc_connect()
 * to actually connect to the server.
 * 
 * @param host Server hostname or IP address (IPv4 only)
 * @param port Server port number (1-65535)
 * @return Pointer to client instance on success, NULL on failure
 * 
 * @note The returned client must be freed with egc_destroy() when no longer needed.
 * @see egc_connect(), egc_destroy()
 */
egc_client_t* egc_create(const char* host, int port);

/**
 * @brief Connect to the chat server with a nickname
 * 
 * Establishes a connection to the server and registers the client with the
 * specified nickname. The connection uses a timeout to prevent hanging.
 * 
 * @param client Valid client instance created with egc_create()
 * @param nickname Desired nickname (must follow validation rules)
 * @return true on successful connection, false on failure
 * 
 * Nickname validation rules:
 * - 1-32 characters long
 * - Must start with alphanumeric character
 * - Only alphanumeric, underscore, and hyphen allowed
 * - No consecutive special characters
 * - Cannot be reserved names (server, admin, system, null, undefined)
 * 
 * @note If connection fails, check network connectivity and server availability.
 * @see egc_create(), egc_send()
 */
bool egc_connect(egc_client_t* client, const char* nickname);

/**
 * @brief Send a chat message to the server
 * 
 * Sends a text message to the chat server, which will broadcast it to all
 * other connected clients. The message is subject to validation and rate limiting.
 * 
 * @param client Connected client instance
 * @param text Message content to send
 * @return true if message was sent successfully, false on failure
 * 
 * Message validation rules:
 * - 1-512 characters long
 * - Only printable ASCII characters (32-126) and spaces allowed
 * - Cannot be only whitespace
 * 
 * Rate limiting:
 * - Maximum 10 messages per second per client
 * - Attempts to exceed rate limit will return false
 * 
 * @note Messages are sent asynchronously. A return value of true indicates
 *       the message was queued for sending, not that it was received by the server.
 * @see egc_connect(), egc_set_message_callback()
 */
bool egc_send(egc_client_t* client, const char* text);

/**
 * @brief Set callback function for receiving messages
 * 
 * Registers a callback function that will be called whenever a message is
 * received from the server. Only one callback can be active at a time.
 * 
 * @param client Valid client instance
 * @param cb Callback function pointer (can be NULL to disable callbacks)
 * @param user_data Arbitrary data pointer passed to callback (can be NULL)
 * 
 * @note The callback may be invoked from a background thread. Ensure your
 *       callback function is thread-safe.
 * @warning Do not call other egc_* functions from within the callback to
 *          avoid potential deadlocks.
 * @see egc_message_callback
 */
void egc_set_message_callback(egc_client_t* client, egc_message_callback cb, void* user_data);

/**
 * @brief Poll for events (legacy function - no-op)
 * 
 * This function exists for API compatibility but performs no operation.
 * The library uses internal threading for all network operations, so
 * explicit polling is not required.
 * 
 * @param client Client instance (can be NULL)
 * @deprecated This function is a no-op. Remove calls to it.
 */
void egc_poll(egc_client_t* client);

/**
 * @brief Destroy client and free resources
 * 
 * Disconnects from the server (if connected), stops all background threads,
 * and frees all resources associated with the client. After calling this
 * function, the client pointer becomes invalid and must not be used.
 * 
 * @param client Client instance to destroy (NULL is safe to pass)
 * 
 * @note This function blocks until all background threads have terminated.
 * @note It's safe to call this function multiple times or with NULL.
 * @see egc_create()
 */
void egc_destroy(egc_client_t* client);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>

/**
 * @namespace egc
 * @brief EasyGameChat C++ API namespace
 * 
 * Contains the C++ interface for EasyGameChat, providing a more modern
 * and type-safe API compared to the C interface. All C++ classes and
 * functions are contained within this namespace.
 */
namespace egc {

// Security-focused constants

/** @brief Maximum allowed nickname length in characters */
const size_t MAX_NICKNAME_LENGTH = 32;

/** @brief Maximum allowed message length in characters */
const size_t MAX_MESSAGE_LENGTH = 512;

/** @brief Maximum internal buffer size for network operations */
const size_t MAX_BUFFER_SIZE = 4096;

/** @brief Connection timeout in milliseconds */
const int CONNECT_TIMEOUT_MS = 5000;

/** @brief Receive operation timeout in milliseconds */
const int RECV_TIMEOUT_MS = 100;

// TLS Config
/** @brief Maximum duration allowed for completing a TLS handshake. */
const int TLS_HANDSHAKE_TIMEOUT_MS = 10000;

/** 
 * @brief Whether to verify the TLS certificate of the remote server.
 * 
 * @note WARNING: This should be set to true in production to ensure secure communication. 
 * Disabling certificate verification makes the connection vulnerable to man-in-the-middle attacks.
 */
const bool VERIFY_TLS_CERTIFICATE = false; // IMPORTANT: Do not forget to set to true for production

/**
 * @brief Cross-platform sleep utility
 * 
 * Suspends execution for the specified number of milliseconds.
 * This function provides consistent behavior across Windows and Unix platforms.
 * 
 * @param ms Number of milliseconds to sleep
 */
inline void sleep_ms(int ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts{};
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, nullptr);
#endif
}

/**
 * @brief Set socket to non-blocking mode
 * 
 * @param sock Socket file descriptor
 * @return true on success, false on failure
 */
inline bool setNonBlocking(int sock) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) return false;
  return (fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1);
#endif
}

/**
 * @brief Set socket to blocking mode
 * 
 * @param sock Socket file descriptor
 * @return true on success, false on failure
 */
inline bool setBlocking(int sock) {
#ifdef _WIN32
  u_long mode = 0;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) return false;
  return (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) != -1);
#endif
}

/**
 * @brief Cross-platform socket close function
 * 
 * @param sock Socket file descriptor to close
 */
inline void closesocket_x(int sock) {
#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif
}

/**
 * @brief Get last socket error code
 * 
 * @return Platform-specific error code
 */
inline int get_last_error() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

/**
 * @brief Validate nickname according to security rules
 * 
 * Checks if a nickname meets all security and formatting requirements.
 * This helps prevent various attacks and ensures consistent user identification.
 * 
 * @param nickname Nickname string to validate
 * @return true if nickname is valid, false otherwise
 * 
 * Validation rules:
 * - Length: 1-32 characters
 * - First character must be alphanumeric
 * - Allowed characters: alphanumeric, underscore, hyphen
 * - No consecutive special characters (_, -)
 * - Not a reserved name (server, admin, system, null, undefined)
 */
inline bool isValidNickname(const std::string& nickname) {
  if (nickname.empty() || nickname.length() > MAX_NICKNAME_LENGTH) {
    return false;
  }
  
  // Must start with alphanumeric
  if (!std::isalnum(nickname[0])) {
    return false;
  }
  
  // Only allow alphanumeric, underscore, hyphen (no consecutive special chars)
  bool lastWasSpecial = false;
  for (char c : nickname) {
    if (!std::isalnum(c) && c != '_' && c != '-') {
      return false;
    }
    bool currentIsSpecial = (c == '_' || c == '-');
    if (currentIsSpecial && lastWasSpecial) {
      return false; // No consecutive special characters
    }
    lastWasSpecial = currentIsSpecial;
  }
  
  // Reserved names check
  std::string lower = nickname;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower == "server" || lower == "admin" || lower == "system" || 
      lower == "null" || lower == "undefined") {
    return false;
  }
  
  return true;
}

/**
 * @brief Validate message content according to security rules
 * 
 * Ensures message content is safe and within acceptable limits.
 * This prevents various attacks and maintains chat quality.
 * 
 * @param message Message string to validate
 * @return true if message is valid, false otherwise
 * 
 * Validation rules:
 * - Length: 1-512 characters
 * - Only printable ASCII (32-126) and space characters allowed
 * - Cannot be only whitespace
 */
inline bool isValidMessage(const std::string& message) {
  if (message.empty() || message.length() > MAX_MESSAGE_LENGTH) {
    return false;
  }
  
  // Strict character validation - only printable ASCII + space
  for (unsigned char c : message) {
    if (c < 32 || c > 126) {
      if (c != ' ') { // Allow spaces
        return false;
      }
    }
  }
  
  // No message can be only whitespace
  if (std::all_of(message.begin(), message.end(), ::isspace)) {
    return false;
  }
  
  return true;
}

/**
 * @brief Validate JSON string for security
 * 
 * Performs basic validation on JSON strings to prevent malformed
 * data from causing issues. This is a lightweight check that
 * complements the full JSON parsing.
 * 
 * @param str JSON string to validate
 * @return true if JSON appears structurally sound, false otherwise
 */
inline bool isSecureJson(const std::string& str) {
  if (str.length() > MAX_BUFFER_SIZE) return false;
  
  // Must start and end with braces
  if (str.empty() || str.front() != '{' || str.back() != '}') {
    return false;
  }
  
  // Count braces to prevent malformed JSON
  int braceCount = 0;
  bool inString = false;
  bool escaped = false;
  
  for (char c : str) {
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
    
    if (!inString) {
      if (c == '{') braceCount++;
      else if (c == '}') braceCount--;
    }
  }
  
  return braceCount == 0 && !inString;
}

class TLSInitializer {
public:
  TLSInitializer() {
#ifndef _WIN32
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
  }

  ~TLSInitializer() {
#ifndef _WIN32
    EVP_cleanup();
    ERR_free_strings();
#endif
  }
};

class TLSSocket {
private:
#ifdef _WIN32
  CtxtHandle context;
  CredHandle credentials;
  bool contextInitialized = false;
  bool credentialsInitialized = false;
  std::vector<char> readBuffer;
  std::vector<char> writeBuffer;
#else
  SSL_CTX* ctx = nullptr;
  SSL* ssl = nullptr;
#endif
  int socket;
  bool connected = false;

public:
  TLSSocket() : socket(-1) {
#ifdef _WIN32
    readBuffer.resize(16384);
    writeBuffer.resize(16384);
#endif
  }

  ~TLSSocket() {
    disconnect();
  }

  bool connect(const std::string& host, int port) {
    socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

#ifdef _WIN32
    if (InetPtonA(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
      closesocket_x(socket);
      return false;
    }
#else
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      closesocket_x(socket);
      return false;
    }
#endif

    if (::connect(socket, (sockaddr*)&addr, sizeof(addr)) != 0) {
      closesocket_x(socket);
      return false;
    }

    if (!performTLSHandshake(host)) {
      closesocket_x(socket);
      return false;
    }

    connected = true;
    return true;
  }

  ssize_t send(const void* data, size_t len) {
    if (!connected) return -1;

#ifdef _WIN32
    return sendWindows(data, len);
#else
    return SSL_write(ssl, data, static_cast<int>(len));
#endif
  }

  ssize_t recv(void* data, size_t len) {
    if (!connected) return -1;

#ifdef _WIN32
    return recvWindows(data, len);
#else
    return SSL_read(ssl, data, static_cast<int>(len));
#endif
  }

  void disconnect() {
    if (connected) {
#ifdef _WIN32
      if (contextInitialized) {
        // Send close notify
        DWORD shutdownType = SCHANNEL_SHUTDOWN;
        SecBuffer shutdownBuffer;
        shutdownBuffer.pvBuffer = &shutdownType;
        shutdownBuffer.cbBuffer = sizeof(shutdownType);
        shutdownBuffer.BufferType = SECBUFFER_TOKEN;
        
        SecBufferDesc shutdownDesc;
        shutdownDesc.ulVersion = SECBUFFER_VERSION;
        shutdownDesc.cBuffers = 1;
        shutdownDesc.pBuffers = &shutdownBuffer;
        
        ApplyControlToken(&context, &shutdownDesc);
        DeleteSecurityContext(&context);
        contextInitialized = false;
      }
      if (credentialsInitialized) {
        FreeCredentialsHandle(&credentials);
        credentialsInitialized = false;
      }
#else
      if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
      }
      if (ctx) {
        SSL_CTX_free(ctx);
        ctx = nullptr;
      }
#endif
      connected = false;
    }

    if (socket >= 0) {
      closesocket_x(socket);
      socket = -1;
    }
  }

  bool isConnected() const { return connected; }
  int getSocket() const { return socket; }

private:
  bool performTLSHandshake(const std::string& host) {
#ifdef _WIN32
    return performWindowsTLSHandshake(host);
#else
    return performOpenSSLHandshake(host);
#endif
  }

#ifdef _WIN32
  bool performWindowsTLSHandshake(const std::string& host) {
    // Initialize Schannel credentials
    SCHANNEL_CRED credData{};
    credData.dwVersion = SCHANNEL_CRED_VERSION;
    credData.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;

    SECURITY_STATUS status = AcquireCredentialsHandle(
      nullptr, const_cast<SEC_CHAR*>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
      nullptr, &credData, nullptr, nullptr, &credentials, nullptr
    );

    if (status != SEC_E_OK) return false;
    credentialsInitialized = true;

    // Perform handshake
    SecBuffer outBuffers[1];
    SecBufferDesc outBufferDesc;
    outBuffers[0].pvBuffer = nullptr;
    outBuffers[0].BufferType = SECBUFFER_TOKEN;
    outBuffers[0].cbBuffer = 0;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = outBuffers;
    outBufferDesc.ulVersion = SECBUFFER_VERSION;

    DWORD contextAttributes;
    status = InitializeSecurityContext(
      &credentials, nullptr, const_cast<SEC_CHAR*>(host.c_str()),
      ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
      ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
      0, SECURITY_NATIVE_DREP, nullptr, 0, &context, &outBufferDesc,
      &contextAttributes, nullptr
    );

    if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK) {
      return false;
    }

    contextInitialized = true;

    // Send initial handshake data
    if (outBuffers[0].cbBuffer > 0) {
      ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer),
        outBuffers[0].cbBuffer, 0);
      FreeContextBuffer(outBuffers[0].pvBuffer);
    }

    // Continue handshake until complete
    return completeWindowsHandshake();
  }

  bool completeWindowsHandshake() {
    std::vector<char> buffer(8192);

    while (true) {
      ssize_t received = ::recv(socket, buffer.data(), buffer.size(), 0);
      if (received <= 0) return false;

      SecBuffer inBuffers[2];
      SecBuffer outBuffers[1];
      SecBufferDesc inBufferDesc, outBufferDesc;

      inBuffers[0].pvBuffer = buffer.data();
      inBuffers[0].cbBuffer = static_cast<unsigned long>(received);
      inBuffers[0].BufferType = SECBUFFER_TOKEN;
      inBuffers[1].pvBuffer = nullptr;
      inBuffers[1].cbBuffer = 0;
      inBuffers[1].BufferType = SECBUFFER_EMPTY;
      inBufferDesc.cBuffers = 2;
      inBufferDesc.pBuffers = inBuffers;
      inBufferDesc.ulVersion = SECBUFFER_VERSION;

      outBuffers[0].pvBuffer = nullptr;
      outBuffers[0].BufferType = SECBUFFER_TOKEN;
      outBuffers[0].cbBuffer = 0;
      outBufferDesc.cBuffers = 1;
      outBufferDesc.pBuffers = outBuffers;
      outBufferDesc.ulVersion = SECBUFFER_VERSION;

      DWORD contextAttributes;
      SECURITY_STATUS status = InitializeSecurityContext(
        &credentials, &context, nullptr,
        ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
        ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
        0, SECURITY_NATIVE_DREP, &inBufferDesc, 0, nullptr, &outBufferDesc,
        &contextAttributes, nullptr
      );

      if (status == SEC_E_OK) {
        if (outBuffers[0].cbBuffer > 0) {
          ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer),
            outBuffers[0].cbBuffer, 0);
          FreeContextBuffer(outBuffers[0].pvBuffer);
        }
        return true; // Handshake complete
      } else if (status == SEC_I_CONTINUE_NEEDED) {
        if (outBuffers[0].cbBuffer > 0) {
          ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer),
            outBuffers[0].cbBuffer, 0);
        }
        continue;
      } else {
        return false;
      }
    }
  }

  ssize_t sendWindows(const void* data, size_t len) {
    SecPkgContext_StreamSizes sizes;
    SECURITY_STATUS status = QueryContextAttributes(&context, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (status != SEC_E_OK) return -1;

    size_t totalSent = 0;
    const char* dataPtr = static_cast<const char*>(data);

    while (totalSent < len) {
      size_t chunkSize = std::min(len - totalSent, static_cast<size_t>(sizes.cbMaximumMessage));

      SecBuffer buffers[4];
      buffers[0].pvBuffer = writeBuffer.data();
      buffers[0].cbBuffer = sizes.cbHeader;
      buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

      buffers[1].pvBuffer = writeBuffer.data() + sizes.cbHeader;
      buffers[1].cbBuffer = static_cast<unsigned long>(chunkSize);
      buffers[1].BufferType = SECBUFFER_DATA;
      memcpy(buffers[1].pvBuffer, dataPtr + totalSent, chunkSize);

      buffers[2].pvBuffer = writeBuffer.data() + sizes.cbHeader + chunkSize;
      buffers[2].cbBuffer = sizes.cbTrailer;
      buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

      buffers[3].pvBuffer = nullptr;
      buffers[3].cbBuffer = 0;
      buffers[3].BufferType = SECBUFFER_EMPTY;

      SecBufferDesc bufferDesc;
      bufferDesc.ulVersion = SECBUFFER_VERSION;
      bufferDesc.cBuffers = 4;
      bufferDesc.pBuffers = buffers;

      status = EncryptMessage(&context, 0, &bufferDesc, 0);
      if (status != SEC_E_OK) return -1;

      size_t totalBytes = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
      ssize_t sent = ::send(socket, writeBuffer.data(), totalBytes, 0);
      if (sent != static_cast<ssize_t>(totalBytes)) return -1;

      totalSent += chunkSize;
    }

    return static_cast<ssize_t>(totalSent);
  }

  ssize_t recvWindows(void* data, size_t len) {
    static std::vector<char> decryptBuffer;
    static size_t decryptBufferUsed = 0;

    if (decryptBuffer.empty()) {
      decryptBuffer.resize(16384);
    }

    // We need to check if /exit was called
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100 * 1000; // 100ms timeout

    int result = select(socket + 1, &readSet, nullptr, nullptr, &timeout);
    if (result < 0) return -1; // select() error
    if (result == 0) return 0; // timeout - no data, retry in recvLoop()

    ssize_t received = ::recv(socket, readBuffer.data(), readBuffer.size(), 0);
    if (received <= 0) return received;

    SecBuffer buffers[4];
    buffers[0].pvBuffer = readBuffer.data();
    buffers[0].cbBuffer = static_cast<unsigned long>(received);
    buffers[0].BufferType = SECBUFFER_DATA;

    buffers[1].BufferType = SECBUFFER_EMPTY;
    buffers[2].BufferType = SECBUFFER_EMPTY;
    buffers[3].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc bufferDesc;
    bufferDesc.ulVersion = SECBUFFER_VERSION;
    bufferDesc.cBuffers = 4;
    bufferDesc.pBuffers = buffers;

    SECURITY_STATUS status = DecryptMessage(&context, &bufferDesc, 0, nullptr);
    if (status != SEC_E_OK) return -1;

    for (int i = 0; i < 4; i++) {
      if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].cbBuffer > 0) {
        size_t copySize = std::min(len, static_cast<size_t>(buffers[i].cbBuffer));
        memcpy(data, buffers[i].pvBuffer, copySize);
        return static_cast<ssize_t>(copySize);
      }
    }

    return 0;
  }

#else
  bool performOpenSSLHandshake(const std::string& host) {
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return false;

    // Configure SSL context
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    if (!VERIFY_TLS_CERTIFICATE) {
      SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    ssl = SSL_new(ctx);
    if (!ssl) return false;

    if (SSL_set_fd(ssl, socket) != 1) return false;

    // Set SNI
    SSL_set_tlsext_host_name(ssl, host.c_str());

    // Perform handshake with timeout
    SSL_set_connect_state(ssl);

    // Set non-blocking for timeout
    if (!setNonBlocking(socket)) return false;

    int result;
    time_t startTime = time(nullptr);

    while (true) {
      result = SSL_connect(ssl);
      if (result == 1) break; // Success

      int error = SSL_get_error(ssl, result);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
        if (time(nullptr) - startTime > TLS_HANDSHAKE_TIMEOUT_MS / 1000) {
          return false; // Timeout
        }
        sleep_ms(10);
        continue;
      } else {
        return false; // Other error
      }
    }

    // Set back to blocking
    if (!setBlocking(socket)) return false;

    return true;
  }
#endif

};

/**
 * @brief RAII wrapper for WinSock initialization
 * 
 * Automatically initializes WinSock on Windows platforms when constructed
 * and cleans up when destroyed. On non-Windows platforms, this class
 * does nothing but provides consistent interface.
 */
class WinSockInitializer {
public:
  /**
   * @brief Initialize WinSock (Windows only)
   * 
   * On Windows, calls WSAStartup() to initialize the WinSock library.
   * On other platforms, this constructor does nothing.
   */
  WinSockInitializer() {
#ifdef _WIN32
    WSADATA wsaData;
    int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != 0) {
      std::cerr << "WSAStartup failed: " << r << std::endl;
    }
#endif
  }

  /**
   * @brief Cleanup WinSock (Windows only)
   * 
   * On Windows, calls WSACleanup() to cleanup the WinSock library.
   * On other platforms, this destructor does nothing.
   */
  ~WinSockInitializer() {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};

/**
 * @brief Main EasyGameChat client class
 * 
 * Provides a modern C++ interface for chat functionality with automatic
 * resource management, thread safety, and security features built-in.
 * 
 * Usage example:
 * @code
 * egc::EasyGameChat chat("127.0.0.1", 3000);
 * chat.setMessageCallback([](const std::string& from, const std::string& text) {
 *     std::cout << from << ": " << text << std::endl;
 * });
 * 
 * if (chat.connect("MyNickname")) {
 *     chat.sendMessage("Hello, world!");
 *     // ... keep application running ...
 *     chat.disconnect(); // Optional - destructor will handle cleanup
 * }
 * @endcode
 */
class EasyGameChat {
public:
  /**
   * @brief Callback function type for message reception
   * 
   * @param from Sender's nickname
   * @param text Message content
   */
  using MessageCallback = std::function<void(const std::string& from, const std::string& text)>;

  /**
   * @brief Construct chat client for specified server
   * 
   * Creates a new chat client configured to connect to the given host and port.
   * No network connection is established until connect() is called.
   * 
   * @param host Server hostname or IP address
   * @param port Server port number
   */
  EasyGameChat(const std::string& host, int port, bool useTLS = true);

  /**
   * @brief Destructor - automatically disconnects and cleans up
   * 
   * Ensures all resources are properly released and background threads
   * are terminated before the object is destroyed.
   */
  ~EasyGameChat();

  /**
   * @brief Connect to server with nickname
   * 
   * Establishes connection to the chat server and attempts to register
   * with the specified nickname. This function includes timeout handling
   * to prevent indefinite blocking.
   * 
   * @param nickname Desired nickname (must pass validation)
   * @return true on successful connection, false on failure
   * 
   * @note If already connected, this function returns false.
   * @see disconnect(), sendMessage()
   */
  bool connect(const std::string& nickname);

  /**
   * @brief Send message to chat
   * 
   * Sends a text message to all other connected clients via the server.
   * Messages are subject to validation and rate limiting.
   * 
   * @param text Message content to send
   * @return true if message was queued for sending, false on failure
   * 
   * Failure reasons:
   * - Not connected to server
   * - Message fails validation (length, content)
   * - Rate limit exceeded (max 10 msgs/second)
   * - Network error
   * 
   * @see connect(), setMessageCallback()
   */
  bool sendMessage(const std::string& text);

  /**
   * @brief Set callback for incoming messages
   * 
   * Registers a callback function that will be invoked whenever a message
   * is received from the server. The callback is called from a background
   * thread, so ensure thread safety.
   * 
   * @param cb Callback function (use nullptr to disable)
   * 
   * @warning The callback is invoked from a background thread. Ensure your
   *          callback is thread-safe and doesn't block for long periods.
   * @note Only one callback can be active at a time. Setting a new callback
   *       replaces the previous one.
   */
  void setMessageCallback(MessageCallback cb);

  /**
   * @brief Disconnect from server
   * 
   * Gracefully disconnects from the server, stops all background threads,
   * and releases network resources. It's safe to call this multiple times.
   * 
   * @note This function blocks until all background operations complete.
   * @note The destructor automatically calls this function.
   */
  void disconnect();

private:
  /**
   * @brief Background thread function for receiving messages
   * 
   * Runs in a separate thread to continuously receive and process
   * messages from the server. Handles JSON parsing, validation,
   * and callback invocation.
   */
  void recvLoop();

  /**
   * @brief Send JSON message to server
   * 
   * Serializes a JSON object and sends it to the server with proper
   * error handling and retry logic.
   * 
   * @param j JSON object to send
   * @return true on success, false on failure
   */
  bool sendJson(const nlohmann::json& j);

  /**
   * @brief Connect to server with timeout
   * 
   * Performs non-blocking connect with specified timeout to prevent
   * the application from hanging on unresponsive servers.
   * 
   * @param sock Socket file descriptor
   * @param addr Server address structure
   * @param addrlen Size of address structure
   * @param timeout_ms Timeout in milliseconds
   * @return true on successful connection, false on failure/timeout
   */
  bool connectWithTimeout(int sock, const sockaddr* addr, socklen_t addrlen, int timeout_ms);

  bool connectWithTLS(const std::string& host, int port);

  // Member variables
  std::string _host;                    // Server hostname/IP
  int _port;                            // Server port number
  std::atomic<int> _sock{-1};           // Socket file descriptor (atomic for thread safety)
  std::string _nickname;                // Client's registered nickname
  std::thread _recvThread;              // Background thread for receiving messages
  std::atomic<bool> _running{false};    // Flag indicating if client is running
  std::atomic<bool> _shouldStop{false}; // Flag to signal thread shutdown
  MessageCallback _callback;            // User-provided message callback
  std::mutex _callbackMutex;            // Protects callback access
  WinSockInitializer _winsockInit;      // RAII WinSock initialization

  // We will probably replace the old socket management with TLS or keep it for legacy support...?
  std::unique_ptr<TLSSocket> _tlsSocket;
  bool _useTLS;
  TLSInitializer _tlsInit;
  
  // Rate limiting members
  std::chrono::steady_clock::time_point _lastSendTime;  // Timestamp of last message sent
  std::mutex _sendMutex;                                // Protects rate limiting state
  static const int MIN_SEND_INTERVAL_MS = 100;          // Minimum interval between messages (ms)
};

// Implementation 

EasyGameChat::EasyGameChat(const std::string& host, int port, bool useTLS)
  : _host(host), _port(port), _useTLS(useTLS), _lastSendTime(std::chrono::steady_clock::now()) {
  if (_useTLS) {
    _tlsSocket = std::make_unique<TLSSocket>();
  }
}

EasyGameChat::~EasyGameChat() { 
  disconnect(); 
}

bool EasyGameChat::connectWithTimeout(int sock, const sockaddr* addr, socklen_t addrlen, int timeout_ms) {
  // Set non-blocking for connect
  if (!setNonBlocking(sock)) {
    return false;
  }
  
  int result = ::connect(sock, addr, addrlen);
  if (result == 0) {
    // Connected immediately
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

  // Wait for connection with timeout
  fd_set writeSet;
  FD_ZERO(&writeSet);
  FD_SET(sock, &writeSet);
  
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  
  int selectResult = select(sock + 1, nullptr, &writeSet, nullptr, &timeout);
  if (selectResult <= 0) {
    return false; // Timeout or error
  }
  
  // Check if connection succeeded
  int error;
  socklen_t errorLen = sizeof(error);
  if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &errorLen) < 0) {
    return false;
  }
  
  if (error != 0) {
    return false;
  }
  
  return setBlocking(sock);
}

bool EasyGameChat::connect(const std::string& nickname) {
  if (_running.load()) return false;
  
  // Validate nickname
  if (!isValidNickname(nickname)) {
    std::cerr << "Invalid nickname format" << std::endl;
    return false;
  }

  _nickname = nickname;

  nlohmann::json j;
  j["from"] = "Client";
  j["text"] = nickname;

  bool connected = false;

  if (_useTLS) {
    connected = _tlsSocket->connect(_host, _port);
    if (connected) {
      _sock.store(_tlsSocket->getSocket());
      if (!sendJson(j)) {
        _tlsSocket->disconnect();
        _sock.store(-1);
        return false;
      }
    }
  }
  else {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
      return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(_port));

#ifdef _WIN32
    if (InetPtonA(AF_INET, _host.c_str(), &addr.sin_addr) != 1) {
      closesocket_x(sock);
      return false;
    }
#else
    if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) <= 0) {
      closesocket_x(sock);
      return false;
    }
#endif

    if (!connectWithTimeout(sock, (sockaddr*)&addr, sizeof(addr), CONNECT_TIMEOUT_MS)) {
      closesocket_x(sock);
      return false;
    }

    if (!setNonBlocking(sock)) {
      closesocket_x(sock);
      return false;
    }

    _sock.store(sock);

    if (!sendJson(j)) {
      closesocket_x(sock);
      _sock.store(-1);
      return false;
    }
  }

  _shouldStop.store(false);
  _running.store(true);
  _recvThread = std::thread(&EasyGameChat::recvLoop, this);
  return true;
}

bool EasyGameChat::sendJson(const nlohmann::json& j) {
  if (_sock.load() < 0) return false;
  
  try {
    std::string jsonStr = j.dump();
    
    // Validate JSON output
    if (!isSecureJson(jsonStr)) {
      return false;
    }
    
    jsonStr += "\n";
    
    ssize_t sent;

    if (_useTLS && _tlsSocket) {
      sent = _tlsSocket->send(jsonStr.c_str(), jsonStr.size());
    } else {
      sent = ::send(_sock.load(), jsonStr.c_str(), jsonStr.size(), 0);
    }

    return sent == static_cast<ssize_t>(jsonStr.size());
  } catch (const std::exception&) {
    return false;
  }
}

void EasyGameChat::recvLoop() {
  //std::cout << "[DEBUG] STARTING recvLoop\n";
  std::vector<char> buffer(MAX_BUFFER_SIZE);
  std::string leftover;
  leftover.reserve(MAX_BUFFER_SIZE);

  while (_running.load() && !_shouldStop.load()) {
    if (_sock.load() < 0) break;

    ssize_t received;

    if (_useTLS && _tlsSocket) {
      //std::cout << "[DEBUG] TLS recv called\n";
      received = _tlsSocket->recv(buffer.data(), buffer.size() - 1);
      //std::cout << "[DEBUG] TLS recv returned " << received << "\n";
    } else {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(_sock.load(), &readSet);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = RECV_TIMEOUT_MS * 1000;

      int selectResult = select(_sock.load() + 1, &readSet, nullptr, nullptr, &timeout);
      if (selectResult < 0) continue;

      ssize_t received = recv(_sock.load(), buffer.data(), buffer.size() - 1, 0);
    }

    if (received > 0) {
      buffer[received] = '\0';

      //std::cerr << "[DEBUG] RAW MESSAGE RECEIVED: " << std::string(buffer.data(), received) << std::endl;
      
      // This should prevent buffer overflow attacks
      if (leftover.size() + static_cast<size_t>(received) > MAX_BUFFER_SIZE) {
        leftover.clear(); // Reset on potential attack
        continue;
      }
      
      leftover.append(buffer.data(), static_cast<size_t>(received));

      size_t pos;
      int messagesProcessed = 0;
      const int maxMessagesPerLoop = 10; // Prevent message flooding
      
      while ((pos = leftover.find('\n')) != std::string::npos && 
             messagesProcessed < maxMessagesPerLoop) {
        std::string line = leftover.substr(0, pos);
        leftover.erase(0, pos + 1);
        messagesProcessed++;

        // Line processing
        if (line.size() > MAX_MESSAGE_LENGTH) continue;
        
        // Trim whitespaces
        while (!line.empty() && std::isspace(line.back())) {
          line.pop_back();
        }
        while (!line.empty() && std::isspace(line.front())) {
          line.erase(0, 1);
        }

        if (isSecureJson(line)) {
          try {
            auto j = nlohmann::json::parse(line);
            
            // Validation
            if (j.is_object() && j.contains("from") && j.contains("text") && 
                j["from"].is_string() && j["text"].is_string() &&
                j.size() == 2) { // Only allow exactly these two fields. Might change in the future.
              
              std::string from = j["from"];
              std::string text = j["text"];
              
              // Double-validate (first this had isValidNickname, but it's getting the server filtered. This will probably need a better solution later.)
              if (/* isValidNickname(from) &&  */isValidMessage(text)) {
                std::lock_guard<std::mutex> lock(_callbackMutex);
                if (_callback) {
                  _callback(from, text);
                }
              }
            }
          } catch (const nlohmann::json::exception&) {
            // Ignore malformed JSON
          }
        }
      }
    }
    else if (received == 0) {
      break; // Connection closed
    }
    else {
      // received < 0
      if (_useTLS) {
        break; // handle TLS-specific errors
      } else {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
#endif
          break; // Real error
        }
      }
    }
  }
}

bool EasyGameChat::sendMessage(const std::string& text) {
  if (_sock.load() < 0 || !_running.load()) return false;
  
  // Validate message
  if (!isValidMessage(text)) {
    return false;
  }
  
  // Rate limiting
  {
    std::lock_guard<std::mutex> lock(_sendMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastSendTime);
    
    if (elapsed.count() < MIN_SEND_INTERVAL_MS) {
      return false; // Rate limited. Might implement a logger for the server.
    }
    
    _lastSendTime = now;
  }

  // Create JSON
  nlohmann::json j;
  j["from"] = _nickname;
  j["text"] = text;
  
  return sendJson(j);
}

void EasyGameChat::setMessageCallback(MessageCallback cb) {
  std::lock_guard<std::mutex> lock(_callbackMutex);
  _callback = std::move(cb);
}

void EasyGameChat::disconnect() {
  if (_running.load()) {
    _shouldStop.store(true);
    _running.store(false);
    
    if (_recvThread.joinable()) {
      _recvThread.join();
    }

    if (_useTLS && _tlsSocket) {
      _tlsSocket->disconnect();
    } else {
      int sock = _sock.exchange(-1);
      if (sock >= 0) {
        closesocket_x(sock);
      }
    }

    _sock.store(-1);
  }
}

} // namespace egc

// C API Implementation

struct egc_client_t {
  egc::EasyGameChat* cpp_client;
  egc_message_callback cb;
  void* user_data;
  std::mutex* callback_mutex; // Prevent use-after-free
  std::atomic<bool>* valid;   // Track if client is still valid
};

extern "C" {

egc_client_t* egc_create(const char* host, int port) {
  if (!host || port <= 0 || port > 65535) return nullptr;
  
  egc_client_t* client = (egc_client_t*)malloc(sizeof(egc_client_t));
  if (!client) return nullptr;
  
  try {
    client->cpp_client = new egc::EasyGameChat(host, port);
    client->cb = nullptr;
    client->user_data = nullptr;
    client->callback_mutex = new std::mutex();
    client->valid = new std::atomic<bool>(true);
    return client;
  } catch (...) {
    free(client);
    return nullptr;
  }
}

bool egc_connect(egc_client_t* client, const char* nickname) {
  if (!client || !client->cpp_client || !nickname || !client->valid->load()) {
    return false;
  }
  return client->cpp_client->connect(nickname);
}

bool egc_send(egc_client_t* client, const char* text) {
  if (!client || !client->cpp_client || !text || !client->valid->load()) {
    return false;
  }
  return client->cpp_client->sendMessage(text);
}

void egc_set_message_callback(egc_client_t* client, egc_message_callback cb, void* user_data) {
  if (!client || !client->cpp_client || !client->valid->load()) return;
  
  std::lock_guard<std::mutex> lock(*client->callback_mutex);
  client->cb = cb;
  client->user_data = user_data;
  
  client->cpp_client->setMessageCallback([client](const std::string& from, const std::string& text) {
    std::lock_guard<std::mutex> lock(*client->callback_mutex);
    if (client->valid->load() && client->cb) {
      client->cb(from.c_str(), text.c_str(), client->user_data);
    }
  });
}

void egc_poll(egc_client_t* client) {
  // No-op - threading handles everything
  (void)client;
}

void egc_destroy(egc_client_t* client) {
  if (!client) return;
  
  // Mark as invalid first
  if (client->valid) {
    client->valid->store(false);
  }
  
  // Wait for any ongoing callbacks to finish
  if (client->callback_mutex) {
    std::lock_guard<std::mutex> lock(*client->callback_mutex);
    // Now safe to cleanup
  }
  
  if (client->cpp_client) {
    client->cpp_client->disconnect();
    delete client->cpp_client;
    client->cpp_client = nullptr;
  }
  
  delete client->callback_mutex;
  delete client->valid;
  free(client);
}

} // extern "C"

#endif

#endif // EASYGAMECHAT_H