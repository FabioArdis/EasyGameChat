#include <easygamechat/easygamechat.h>

#include <easygamechat/easygamechat.hpp>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>

struct egc_client_t {
  egc::EasyGameChat* cpp_client;
  egc_message_callback cb;
  void* user_data;
  std::mutex* callback_mutex;
  std::atomic<bool>* valid;
};

extern "C" {

egc_client_t* egc_create(const char* host, int port) {
  if (!host || port <= 0 || port > 65535) {
    return nullptr;
  }

  egc_client_t* client = static_cast<egc_client_t*>(malloc(sizeof(egc_client_t)));
  if (!client) {
    return nullptr;
  }

  client->cpp_client = nullptr;
  client->cb = nullptr;
  client->user_data = nullptr;
  client->callback_mutex = nullptr;
  client->valid = nullptr;

  try {
    client->cpp_client = new egc::EasyGameChat(host, port);
    client->callback_mutex = new std::mutex();
    client->valid = new std::atomic<bool>(true);
    return client;
  } catch (...) {
    delete client->cpp_client;
    delete client->callback_mutex;
    delete client->valid;
    free(client);
    return nullptr;
  }
}

bool egc_connect(egc_client_t* client, const char* nickname, const char* token) {
  if (!client || !client->cpp_client || !nickname || !token || !client->valid->load()) {
    return false;
  }
  return client->cpp_client->connect(nickname, token);
}

bool egc_send(egc_client_t* client, const char* text) {
  if (!client || !client->cpp_client || !text || !client->valid->load()) {
    return false;
  }
  return client->cpp_client->sendMessage(text);
}

void egc_set_message_callback(egc_client_t* client, egc_message_callback cb, void* user_data) {
  if (!client || !client->cpp_client || !client->valid->load()) {
    return;
  }

  std::lock_guard<std::mutex> lock(*client->callback_mutex);
  client->cb = cb;
  client->user_data = user_data;

  client->cpp_client->setMessageCallback([client](const std::string& from, const std::string& text) {
    std::lock_guard<std::mutex> callbackLock(*client->callback_mutex);
    if (client->valid->load() && client->cb) {
      client->cb(from.c_str(), text.c_str(), client->user_data);
    }
  });
}

void egc_poll(egc_client_t* client) {
  (void)client;
}

void egc_destroy(egc_client_t* client) {
  if (!client) {
    return;
  }

  if (client->valid) {
    client->valid->store(false);
  }

  if (client->callback_mutex) {
    std::lock_guard<std::mutex> lock(*client->callback_mutex);
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

bool egc_get_connection_info(egc_client_t* client, egc_connection_info_t* info) {
  if (!client || !client->cpp_client || !info || !client->valid->load()) {
    return false;
  }

  try {
    auto cppInfo = client->cpp_client->getConnectionInfo();

    info->connected = cppInfo.connected;

    strncpy(info->host, cppInfo.host.c_str(), sizeof(info->host) - 1);
    info->host[sizeof(info->host) - 1] = '\0';

    info->port = cppInfo.port;

    strncpy(info->nickname, cppInfo.nickname.c_str(), sizeof(info->nickname) - 1);
    info->nickname[sizeof(info->nickname) - 1] = '\0';

    info->tls_enabled = cppInfo.tls_enabled;

    strncpy(info->tls_cipher, cppInfo.tls_cipher.c_str(), sizeof(info->tls_cipher) - 1);
    info->tls_cipher[sizeof(info->tls_cipher) - 1] = '\0';

    strncpy(info->tls_version, cppInfo.tls_version.c_str(), sizeof(info->tls_version) - 1);
    info->tls_version[sizeof(info->tls_version) - 1] = '\0';

    info->tls_bits = cppInfo.tls_bits;

    strncpy(info->server_cert_cn, cppInfo.server_cert_cn.c_str(), sizeof(info->server_cert_cn) - 1);
    info->server_cert_cn[sizeof(info->server_cert_cn) - 1] = '\0';

    info->server_cert_valid = cppInfo.server_cert_valid;

    strncpy(info->tls_error, cppInfo.tls_error.c_str(), sizeof(info->tls_error) - 1);
    info->tls_error[sizeof(info->tls_error) - 1] = '\0';

    return true;
  } catch (...) {
    return false;
  }
}

bool egc_should_stop(egc_client_t* client) {
  if (!client || !client->cpp_client || !client->valid->load()) {
    return false;
  }

  return client->cpp_client->shouldStop();
}

}  // extern "C"
