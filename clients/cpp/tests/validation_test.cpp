#include <easygamechat/easygamechat.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    failures++;
  }
}

}  // namespace

int main() {
  egc::TLSConfig tlsConfig;
  expect(tlsConfig.verifyCertificate, "tls verification default");

  expect(egc::isValidNickname("alice"), "nickname simple");
  expect(egc::isValidNickname("Alice-1"), "nickname letters and digits");
  expect(egc::isValidNickname("a_b"), "nickname underscore");
  expect(egc::isValidNickname(std::string(egc::MAX_NICKNAME_LENGTH, 'a')), "nickname maximum length");
  expect(egc::isValidNickname("server1"), "nickname reserved prefix");

  expect(!egc::isValidNickname(""), "nickname empty");
  expect(!egc::isValidNickname(std::string(egc::MAX_NICKNAME_LENGTH + 1, 'a')), "nickname too long");
  expect(!egc::isValidNickname("-alice"), "nickname starts with hyphen");
  expect(!egc::isValidNickname("alice!"), "nickname invalid character");
  expect(!egc::isValidNickname("a__b"), "nickname consecutive underscores");
  expect(!egc::isValidNickname("a-_b"), "nickname consecutive mixed specials");
  expect(!egc::isValidNickname("admin"), "nickname reserved");
  expect(!egc::isValidNickname("SERVER"), "nickname reserved different case");
  expect(!egc::isValidNickname("al ice"), "nickname space");

  expect(egc::isValidMessage("hello"), "message plain");
  expect(egc::isValidMessage("hello world"), "message spaces");
  expect(egc::isValidMessage(std::string(egc::MAX_MESSAGE_LENGTH, 'a')), "message maximum length");
  expect(egc::isValidMessage("{}[]~"), "message punctuation");

  expect(!egc::isValidMessage(""), "message empty");
  expect(!egc::isValidMessage("   "), "message whitespace");
  expect(!egc::isValidMessage(std::string(egc::MAX_MESSAGE_LENGTH + 1, 'a')), "message too long");
  expect(!egc::isValidMessage("hello\nworld"), "message newline");
  expect(!egc::isValidMessage("hello\tworld"), "message tab");
  expect(!egc::isValidMessage("caf\xc3\xa9"), "message non-ascii");
  expect(!egc::isValidMessage("hello\x7f"), "message delete character");

  expect(egc::isSecureJson(R"({"from":"alice","text":"hello"})"), "json object");
  expect(egc::isSecureJson(R"({"text":"}"})"), "json brace in string");
  expect(egc::isSecureJson(R"({"text":"\"}"})"), "json escaped quote");

  expect(!egc::isSecureJson(""), "json empty");
  expect(!egc::isSecureJson("not json"), "json not an object");
  expect(!egc::isSecureJson(R"({"text":"hello")"), "json unclosed object");
  expect(!egc::isSecureJson(R"({"text":"hello"}})"), "json extra closing brace");
  expect(!egc::isSecureJson(R"({"text":"hello})"), "json unterminated string");
  expect(!egc::isSecureJson("{" + std::string(egc::MAX_BUFFER_SIZE, 'a') + "}"), "json oversized");

  return failures == 0 ? 0 : 1;
}
