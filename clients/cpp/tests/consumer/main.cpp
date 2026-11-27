#include <easygamechat/easygamechat.hpp>

int main() {
  egc::TLSConfig config;
  return egc::isValidNickname("alice") && config.verifyCertificate ? 0 : 1;
}
