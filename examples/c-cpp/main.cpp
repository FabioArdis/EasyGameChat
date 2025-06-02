/*
* Important Note
* 
* Incoming messages are displayed asynchronously, meaning they can overlap with the
* input prompt while the user is typing. The EasyGameChat library doesn't handle 
* this overlap, leaving console interface management to the programmer.
*/

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

  std::cout << "Connected! Write messages and press ENTER to send them (write '/exit' to exit)\n";

  std::string input;
  while (true) {
    std::getline(std::cin, input);
    if (input == "/exit") break;
    egc_send(client, input.c_str());
  }

  egc_destroy(client);
  std::cout << "Disconnected.\n";
  return 0;
}