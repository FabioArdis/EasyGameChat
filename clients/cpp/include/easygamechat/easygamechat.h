#ifndef EASYGAMECHAT_H
#define EASYGAMECHAT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool connected;
  char host[256];
  int port;
  char nickname[64];
  bool tls_enabled;
  char tls_cipher[128];
  char tls_version[32];
  int tls_bits;
  char server_cert_cn[256];
  bool server_cert_valid;
  char tls_error[512];
} egc_connection_info_t;

typedef void (*egc_message_callback)(const char* from, const char* text, void* user_data);

typedef struct egc_client_t egc_client_t;

egc_client_t* egc_create(const char* host, int port);
bool egc_connect(egc_client_t* client, const char* nickname, const char* token);
bool egc_send(egc_client_t* client, const char* text);
void egc_set_message_callback(egc_client_t* client, egc_message_callback cb, void* user_data);
void egc_poll(egc_client_t* client);
void egc_destroy(egc_client_t* client);
bool egc_get_connection_info(egc_client_t* client, egc_connection_info_t* info);
bool egc_should_stop(egc_client_t* client);

#ifdef __cplusplus
}
#endif

#endif
