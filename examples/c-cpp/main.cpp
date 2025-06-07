/*
* Important Note
* 
* Incoming messages are displayed asynchronously, meaning they can overlap with the
* input prompt while the user is typing. The EasyGameChat library doesn't handle 
* this overlap, leaving console interface management to the programmer.
*/

#if defined(_WIN32) && !defined(SECURITY_WIN32)
#define SECURITY_WIN32
#endif

#include "../../clients/c-cpp/EasyGameChat.h"

#include <iostream>
#include <string>
#include <thread>

int main() {
  std::string nickname;
  std::cout << "Insert your nickname: ";
  std::getline(std::cin, nickname);

  auto* client = egc_create("127.0.0.1", 3000);
  if (!client) {
    std::cerr << "Error: could not create client\n";
    return 1;
  }

  if (!egc_connect(client, nickname.c_str())) {
    std::cerr << "Error: could not connect to the server\n";
    egc_destroy(client);
    return 1;
  }

  egc_set_message_callback(client, [](const char* from, const char* text, void*){
    std::cout << "[" << from << "]: " << text << "\n";
  }, nullptr);

  std::cout << "Connected! Write messages and press ENTER to send them (write '/exit' to exit, '/info' for connection info)\n";

  std::string input;
  while (true) {
    std::getline(std::cin, input);
    if (input == "/exit") break;
    if (input == "/info") {
      egc_connection_info_t info;
      if (egc_get_connection_info(client, &info)) {
        // This is just my implementation, of course the user can use the data as they see fit.
        std::cout << "\n=== Connection Info ===" << std::endl;
        std::cout << "Connected: " << (info.connected ? "Yes" : "No") << std::endl;
        std::cout << "Host: " << info.host << std::endl;
        std::cout << "Port: " << info.port << std::endl;
        std::cout << "Nickname: " << info.nickname << std::endl;
        std::cout << "TLS Enabled: " << (info.tls_enabled ? "Yes" : "No") << std::endl;
        
        if (info.tls_enabled) {
          if (strlen(info.tls_cipher) > 0) {
            std::cout << "TLS Cipher: " << info.tls_cipher << std::endl;
          }
          if (strlen(info.tls_version) > 0) {
            std::cout << "TLS Version: " << info.tls_version << std::endl;
          }
          if (info.tls_bits > 0) {
            std::cout << "Key Bits: " << info.tls_bits << std::endl;
          }
          if (strlen(info.server_cert_cn) > 0) {
            std::cout << "Server Certificate CN: " << info.server_cert_cn << std::endl;
          }
          std::cout << "Certificate Valid: " << (info.server_cert_valid ? "Yes" : "No") << std::endl;
          
          if (strlen(info.tls_error) > 0) {
            std::cout << "TLS Error: " << info.tls_error << std::endl;
          }
        }
        std::cout << "=====================\n" << std::endl;
      } else {
        std::cout << "Error: Could not retrieve connection info" << std::endl;
      }
      continue; // Don't send "/info" as a message
    }
    egc_send(client, input.c_str());
  }
  
  egc_destroy(client);
  std::cout << "Disconnected.\n";
  return 0;
}