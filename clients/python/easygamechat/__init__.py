from .easygamechat import (
  EasyGameChat,
  EasyGameChatError,
  ValidationError,
  ConnectionError,
  egc_create,
  egc_connect,
  egc_send,
  egc_set_message_callback,
  egc_destroy,
  is_valid_nickname,
  is_valid_message
)

__version__ = "0.1.0"
__all__ = [
    "EasyGameChat",
    "EasyGameChatError", 
    "ValidationError",
    "ConnectionError",
    "egc_create",
    "egc_connect", 
    "egc_send",
    "egc_set_message_callback",
    "egc_destroy",
    "is_valid_nickname",
    "is_valid_message"
]