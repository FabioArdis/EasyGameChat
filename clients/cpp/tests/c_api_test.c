#include <easygamechat/easygamechat.h>

#include <stddef.h>

int main(void) {
  if (egc_create(NULL, 3000) != NULL) {
    return 1;
  }
  if (egc_create("127.0.0.1", 0) != NULL) {
    return 1;
  }

  egc_client_t* client = egc_create("127.0.0.1", 3000);
  if (!client) {
    return 1;
  }
  if (!egc_set_tls_verification(client, false)) {
    return 5;
  }

  egc_connection_info_t info;
  if (!egc_get_connection_info(client, &info)) {
    return 2;
  }
  if (info.connected) {
    return 3;
  }
  if (egc_should_stop(client)) {
    return 4;
  }

  egc_destroy(client);
  return 0;
}
