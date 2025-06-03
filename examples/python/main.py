#!/usr/bin/env python3
"""
EasyGameChat Python Example
"""

import sys
import time
from pathlib import Path

# Remember to install the library before importing it

from easygamechat import EasyGameChat

def main():
  # Get nickname from user
  nickname = input("Insert your nickname: ").strip()
  
  # Create client
  client = EasyGameChat("127.0.0.1", 3000)
  
  # Set up message callback
  def on_message(from_user: str, text: str):
    print(f"[{from_user}]: {text}")
  
  client.set_message_callback(on_message)
  
  # Connect to server
  if not client.connect(nickname):
    print("Error: could not connect to the server")
    return 1
  
  print("Connected! Write messages and press ENTER to send them (write '/exit' to exit)")
  
  try:
    while True:
      message = input().strip()
      if message == "/exit":
        break
      if message:  # Don't send empty messages
        if not client.send_message(message):
          print("Warning: Failed to send message (might be rate limited or invalid)")
  except KeyboardInterrupt:
    print("\nInterrupted by user")
  except EOFError:
    print("\nEnd of input")
  finally:
    client.disconnect()
    print("Disconnected.")
  
  return 0

if __name__ == "__main__":
  sys.exit(main())