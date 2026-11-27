#include "tls_socket.hpp"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <codecvt>
#include <locale>
#else
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#endif

namespace egc {
namespace {

constexpr int kConnectTimeoutMs = 5000;
constexpr int kRecvTimeoutMs = 100;
constexpr int kTlsHandshakeTimeoutMs = 10000;

}  // namespace

TLSInitializer::TLSInitializer() {
#ifndef _WIN32
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
#endif
}

TLSInitializer::~TLSInitializer() = default;

WinSockInitializer::WinSockInitializer() {
#ifdef _WIN32
  WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

WinSockInitializer::~WinSockInitializer() {
#ifdef _WIN32
  WSACleanup();
#endif
}

TLSSocket::TLSSocket(const TLSConfig& config)
    : verifyCertificate_(config.verifyCertificate), socket(-1) {
#ifdef _WIN32
  readBuffer.resize(16384);
  writeBuffer.resize(16384);
#endif
}

TLSSocket::~TLSSocket() {
  disconnect();
}

bool TLSSocket::connect(const std::string& host, int port) {
  socket = detail::connectTcp(host, port, kConnectTimeoutMs);
  if (socket < 0) {
    return false;
  }

  if (!performTLSHandshake(host)) {
    disconnect();
    return false;
  }

  connected = true;
  return true;
}

ssize_t TLSSocket::send(const void* data, std::size_t len) {
  if (!connected) {
    return -1;
  }

#ifdef _WIN32
  return sendWindows(data, len);
#else
  return SSL_write(ssl, data, static_cast<int>(len));
#endif
}

ssize_t TLSSocket::recv(void* data, std::size_t len) {
  if (!connected) {
    return -1;
  }

#ifdef _WIN32
  return recvWindows(data, len);
#else
  int fd = SSL_get_fd(ssl);
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = kRecvTimeoutMs * 1000;

  int ready = select(fd + 1, &readfds, nullptr, nullptr, &timeout);
  if (ready > 0) {
    return SSL_read(ssl, data, static_cast<int>(len));
  }
  if (ready == 0) {
    errno = EAGAIN;
  }
  return -1;
#endif
}

void TLSSocket::disconnect() {
#ifdef _WIN32
  if (contextInitialized) {
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
    if (connected) {
      SSL_shutdown(ssl);
    }
    SSL_free(ssl);
    ssl = nullptr;
  }
  if (ctx) {
    SSL_CTX_free(ctx);
    ctx = nullptr;
  }
#endif

  if (socket >= 0) {
    detail::closeSocket(socket);
    socket = -1;
  }
  connected = false;
}

bool TLSSocket::isConnected() const {
  return connected;
}

int TLSSocket::getSocket() const {
  return socket;
}

ConnectionInfo TLSSocket::getConnectionInfo(ConnectionInfo info) const {
  if (!connected) {
    info.tls_error = "Not connected";
    return info;
  }

#ifdef _WIN32
  return getWindowsConnectionInfo(info);
#else
  return getOpenSSLConnectionInfo(info);
#endif
}

bool TLSSocket::performTLSHandshake(const std::string& host) {
#ifdef _WIN32
  return performWindowsTLSHandshake(host);
#else
  return performOpenSSLHandshake(host);
#endif
}

#ifdef _WIN32
bool TLSSocket::performWindowsTLSHandshake(const std::string& host) {
  SCHANNEL_CRED credData{};
  credData.dwVersion = SCHANNEL_CRED_VERSION;
  credData.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
  if (!verifyCertificate_) {
    credData.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
  }

  SECURITY_STATUS status = AcquireCredentialsHandle(
      nullptr, const_cast<SEC_CHAR*>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
      nullptr, &credData, nullptr, nullptr, &credentials, nullptr);

  if (status != SEC_E_OK) {
    return false;
  }
  credentialsInitialized = true;

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
      &contextAttributes, nullptr);

  if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK) {
    return false;
  }

  contextInitialized = true;

  if (outBuffers[0].cbBuffer > 0) {
    ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer), outBuffers[0].cbBuffer, 0);
    FreeContextBuffer(outBuffers[0].pvBuffer);
  }

  return completeWindowsHandshake();
}

bool TLSSocket::completeWindowsHandshake() {
  std::vector<char> buffer(8192);

  while (true) {
    ssize_t received = ::recv(socket, buffer.data(), buffer.size(), 0);
    if (received <= 0) {
      return false;
    }

    SecBuffer inBuffers[2];
    SecBuffer outBuffers[1];
    SecBufferDesc inBufferDesc;
    SecBufferDesc outBufferDesc;

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
        &contextAttributes, nullptr);

    if (status == SEC_E_OK) {
      if (outBuffers[0].cbBuffer > 0) {
        ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer), outBuffers[0].cbBuffer, 0);
        FreeContextBuffer(outBuffers[0].pvBuffer);
      }
      return true;
    }
    if (status == SEC_I_CONTINUE_NEEDED) {
      if (outBuffers[0].cbBuffer > 0) {
        ::send(socket, static_cast<char*>(outBuffers[0].pvBuffer), outBuffers[0].cbBuffer, 0);
      }
      continue;
    }
    return false;
  }
}

ssize_t TLSSocket::sendWindows(const void* data, std::size_t len) {
  SecPkgContext_StreamSizes sizes;
  SECURITY_STATUS status = QueryContextAttributes(&context, SECPKG_ATTR_STREAM_SIZES, &sizes);
  if (status != SEC_E_OK) {
    return -1;
  }

  std::size_t totalSent = 0;
  const char* dataPtr = static_cast<const char*>(data);

  while (totalSent < len) {
    std::size_t chunkSize = std::min(len - totalSent, static_cast<std::size_t>(sizes.cbMaximumMessage));

    SecBuffer buffers[4];
    buffers[0].pvBuffer = writeBuffer.data();
    buffers[0].cbBuffer = sizes.cbHeader;
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

    buffers[1].pvBuffer = writeBuffer.data() + sizes.cbHeader;
    buffers[1].cbBuffer = static_cast<unsigned long>(chunkSize);
    buffers[1].BufferType = SECBUFFER_DATA;
    std::memcpy(buffers[1].pvBuffer, dataPtr + totalSent, chunkSize);

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
    if (status != SEC_E_OK) {
      return -1;
    }

    std::size_t totalBytes = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
    ssize_t sent = ::send(socket, writeBuffer.data(), static_cast<int>(totalBytes), 0);
    if (sent != static_cast<ssize_t>(totalBytes)) {
      return -1;
    }

    totalSent += chunkSize;
  }

  return static_cast<ssize_t>(totalSent);
}

ssize_t TLSSocket::recvWindows(void* data, std::size_t len) {
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(socket, &readSet);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = kRecvTimeoutMs * 1000;

  int result = select(socket + 1, &readSet, nullptr, nullptr, &timeout);
  if (result < 0) {
    return -1;
  }
  if (result == 0) {
    WSASetLastError(WSAEWOULDBLOCK);
    return -1;
  }

  ssize_t received = ::recv(socket, readBuffer.data(), static_cast<int>(readBuffer.size()), 0);
  if (received <= 0) {
    return received;
  }

  SecBuffer buffers[4];
  buffers[0].pvBuffer = readBuffer.data();
  buffers[0].cbBuffer = static_cast<unsigned long>(received);
  buffers[0].BufferType = SECBUFFER_DATA;

  for (int i = 1; i < 4; ++i) {
    buffers[i].BufferType = SECBUFFER_EMPTY;
    buffers[i].pvBuffer = nullptr;
    buffers[i].cbBuffer = 0;
  }

  SecBufferDesc bufferDesc;
  bufferDesc.ulVersion = SECBUFFER_VERSION;
  bufferDesc.cBuffers = 4;
  bufferDesc.pBuffers = buffers;

  SECURITY_STATUS status = DecryptMessage(&context, &bufferDesc, 0, nullptr);
  if (status != SEC_E_OK) {
    return -1;
  }

  for (int i = 0; i < 4; i++) {
    if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].cbBuffer > 0) {
      std::size_t copySize = std::min(len, static_cast<std::size_t>(buffers[i].cbBuffer));
      std::memcpy(data, buffers[i].pvBuffer, copySize);
      return static_cast<ssize_t>(copySize);
    }
  }

  return 0;
}

ConnectionInfo TLSSocket::getWindowsConnectionInfo(ConnectionInfo info) const {
  if (!contextInitialized) {
    info.tls_error = "TLS context not initialized";
    return info;
  }

  try {
    SecPkgContext_CipherInfo cipherInfo;
    SECURITY_STATUS status = QueryContextAttributes(
        const_cast<CtxtHandle*>(&context), SECPKG_ATTR_CIPHER_INFO, &cipherInfo);

    if (status == SEC_E_OK) {
      std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
      info.tls_cipher = converter.to_bytes(cipherInfo.szCipherSuite);
      info.tls_bits = static_cast<int>(cipherInfo.dwCipherLen);
    }

    SecPkgContext_ConnectionInfo connInfo;
    status = QueryContextAttributes(
        const_cast<CtxtHandle*>(&context), SECPKG_ATTR_CONNECTION_INFO, &connInfo);

    if (status == SEC_E_OK) {
      switch (connInfo.dwProtocol) {
        case SP_PROT_TLS1_2_CLIENT:
          info.tls_version = "TLSv1.2";
          break;
        case SP_PROT_TLS1_3_CLIENT:
          info.tls_version = "TLSv1.3";
          break;
        default:
          info.tls_version = "Unknown";
          break;
      }
    }

    PCCERT_CONTEXT certContext = nullptr;
    status = QueryContextAttributes(
        const_cast<CtxtHandle*>(&context), SECPKG_ATTR_REMOTE_CERT_CONTEXT, (void*)&certContext);

    if (status == SEC_E_OK && certContext) {
      char cn[256];
      DWORD cnSize = sizeof(cn);

      info.server_cert_valid = verifyCertificate_;
      if (CertGetNameStringA(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, cn, cnSize) > 1) {
        info.server_cert_cn = cn;
      }

      CertFreeCertificateContext(certContext);
    }
  } catch (const std::exception& e) {
    info.tls_error = std::string("TLS info error: ") + e.what();
  }

  return info;
}
#else
bool TLSSocket::performOpenSSLHandshake(const std::string& host) {
  ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    return false;
  }

  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  SSL_CTX_set_verify(ctx, verifyCertificate_ ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);

  if (verifyCertificate_ && SSL_CTX_set_default_verify_paths(ctx) != 1) {
    return false;
  }

  ssl = SSL_new(ctx);
  if (!ssl) {
    return false;
  }

  if (SSL_set_fd(ssl, socket) != 1) {
    return false;
  }

  SSL_set_tlsext_host_name(ssl, host.c_str());

  if (verifyCertificate_) {
    unsigned char address[16];
    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);

    if (inet_pton(AF_INET, host.c_str(), address) == 1 ||
        inet_pton(AF_INET6, host.c_str(), address) == 1) {
      if (X509_VERIFY_PARAM_set1_ip_asc(param, host.c_str()) != 1) {
        return false;
      }
    } else {
      X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
      if (X509_VERIFY_PARAM_set1_host(param, host.c_str(), 0) != 1) {
        return false;
      }
    }
  }

  SSL_set_connect_state(ssl);

  if (!detail::setNonBlocking(socket)) {
    return false;
  }

  time_t startTime = time(nullptr);

  while (true) {
    int result = SSL_connect(ssl);
    if (result == 1) {
      break;
    }

    int error = SSL_get_error(ssl, result);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
      if (time(nullptr) - startTime > kTlsHandshakeTimeoutMs / 1000) {
        return false;
      }
      detail::sleepMs(10);
      continue;
    }
    return false;
  }

  return detail::setBlocking(socket);
}

ConnectionInfo TLSSocket::getOpenSSLConnectionInfo(ConnectionInfo info) const {
  if (!ssl) {
    info.tls_error = "SSL not initialized";
    return info;
  }

  try {
    const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl);
    if (cipher) {
      info.tls_cipher = SSL_CIPHER_get_name(cipher);
      info.tls_bits = SSL_CIPHER_get_bits(cipher, nullptr);
    }

    const char* version = SSL_get_version(ssl);
    if (version) {
      info.tls_version = version;
    }

    bool hasCertificate = false;
    X509* cert = SSL_get_peer_certificate(ssl);
    if (cert) {
      hasCertificate = true;

      const X509_NAME* subject = X509_get_subject_name(cert);
      if (subject) {
        int commonNameIndex = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
        if (commonNameIndex >= 0) {
          const X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject, commonNameIndex);
          if (entry) {
            const ASN1_STRING* commonNameData = X509_NAME_ENTRY_get_data(entry);
            unsigned char* commonNameUtf8 = nullptr;
            int len = ASN1_STRING_to_UTF8(&commonNameUtf8, commonNameData);
            if (len > 0) {
              info.server_cert_cn.assign(
                  reinterpret_cast<const char*>(commonNameUtf8), static_cast<std::size_t>(len));
            }
            if (commonNameUtf8) {
              OPENSSL_free(commonNameUtf8);
            }
          }
        }
      }

      X509_free(cert);
    }

    long verifyResult = SSL_get_verify_result(ssl);
    info.server_cert_valid = verifyCertificate_ && hasCertificate && verifyResult == X509_V_OK;
    if (verifyCertificate_ && verifyResult != X509_V_OK) {
      info.tls_error =
          "Certificate verification failed: " + std::string(X509_verify_cert_error_string(verifyResult));
    }
  } catch (const std::exception& e) {
    info.tls_error = std::string("OpenSSL info error: ") + e.what();
  }

  return info;
}
#endif

}  // namespace egc
