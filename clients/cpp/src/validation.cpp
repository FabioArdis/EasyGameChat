#include <easygamechat/easygamechat.hpp>

#include <algorithm>
#include <cctype>

namespace egc {

bool isValidNickname(const std::string& nickname) {
  if (nickname.empty() || nickname.size() > MAX_NICKNAME_LENGTH) {
    return false;
  }

  if (!std::isalnum(static_cast<unsigned char>(nickname[0]))) {
    return false;
  }

  bool lastWasSpecial = false;
  for (char ch : nickname) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (!std::isalnum(c) && c != '_' && c != '-') {
      return false;
    }
    bool currentIsSpecial = c == '_' || c == '-';
    if (currentIsSpecial && lastWasSpecial) {
      return false;
    }
    lastWasSpecial = currentIsSpecial;
  }

  std::string lower = nickname;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  const char* reserved[] = {"server", "admin", "system", "null", "undefined"};
  for (const char* name : reserved) {
    if (lower == name) {
      return false;
    }
  }

  return true;
}

bool isValidMessage(const std::string& message) {
  if (message.empty() || message.size() > MAX_MESSAGE_LENGTH) {
    return false;
  }

  for (char ch : message) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (c < 32 || c > 126) {
      return false;
    }
  }

  return message.find_first_not_of(' ') != std::string::npos;
}

bool isSecureJson(const std::string& str) {
  if (str.size() > MAX_BUFFER_SIZE) {
    return false;
  }
  if (str.empty() || str.front() != '{' || str.back() != '}') {
    return false;
  }

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
    if (inString) {
      continue;
    }
    if (c == '{') {
      braceCount++;
    } else if (c == '}') {
      braceCount--;
    }
  }

  return braceCount == 0 && !inString;
}

}  // namespace egc
