"""
EasyGameChat Python Client Library
A secure, feature-complete port of the C++ EasyGameChat library
"""

import socket
import json
import threading
import time
import re
import string
from typing import Optional, Callable, Dict, Any
from dataclasses import dataclass
import logging

# Constants
MAX_NICKNAME_LENGTH = 32
MAX_MESSAGE_LENGTH = 512
MAX_BUFFER_SIZE = 4096
CONNECT_TIMEOUT_MS = 5000
RECV_TIMEOUT_MS = 100
MIN_SEND_INTERVAL_MS = 100  # Max 10 messages per second

class EasyGameChatError(Exception):
    """Base exception for EasyGameChat errors"""
    pass

class ValidationError(EasyGameChatError):
    """Raised when input validation fails"""
    pass

class ConnectionError(EasyGameChatError):
    """Raised when connection operations fail"""
    pass

def is_valid_nickname(nickname: str) -> bool:
    """Validate nickname according to security rules"""
    if not nickname or len(nickname) > MAX_NICKNAME_LENGTH:
        return False
    
    # Must start with alphanumeric
    if not nickname[0].isalnum():
        return False
    
    # Only allow alphanumeric, underscore, hyphen (no consecutive special chars)
    last_was_special = False
    for char in nickname:
        if not (char.isalnum() or char in '_-'):
            return False
        current_is_special = char in '_-'
        if current_is_special and last_was_special:
            return False  # No consecutive special characters
        last_was_special = current_is_special
    
    # Reserved names check
    lower = nickname.lower()
    reserved_names = {'server', 'admin', 'system', 'null', 'undefined'}
    if lower in reserved_names:
        return False
    
    return True

def is_valid_message(message: str) -> bool:
    """Validate message according to security rules"""
    if not message or len(message) > MAX_MESSAGE_LENGTH:
        return False
    
    # Strict character validation - only printable ASCII + space
    for char in message:
        char_code = ord(char)
        if char_code < 32 or char_code > 126:
            if char != ' ':  # Allow spaces
                return False
    
    # No message can be only whitespace
    if message.isspace():
        return False
    
    return True

def is_secure_json(json_str: str) -> bool:
    """Validate JSON string for security"""
    if len(json_str) > MAX_BUFFER_SIZE:
        return False
    
    # Must start and end with braces
    if not json_str or not (json_str.startswith('{') and json_str.endswith('}')):
        return False
    
    # Count braces to prevent malformed JSON
    brace_count = 0
    in_string = False
    escaped = False
    
    for char in json_str:
        if escaped:
            escaped = False
            continue
        
        if char == '\\' and in_string:
            escaped = True
            continue
        
        if char == '"':
            in_string = not in_string
            continue
        
        if not in_string:
            if char == '{':
                brace_count += 1
            elif char == '}':
                brace_count -= 1
    
    return brace_count == 0 and not in_string

class EasyGameChat:
    """
    EasyGameChat Python client with security features and rate limiting.
    
    Example usage:
        client = EasyGameChat("127.0.0.1", 3000)
        
        def on_message(from_user, text):
            print(f"[{from_user}]: {text}")
        
        client.set_message_callback(on_message)
        
        if client.connect("MyNickname"):
            client.send_message("Hello, world!")
            time.sleep(5)  # Keep connection alive
            client.disconnect()
    """
    
    def __init__(self, host: str, port: int):
        """
        Initialize EasyGameChat client.
        
        Args:
            host: Server hostname or IP address
            port: Server port number
        """
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.nickname = ""
        self.running = False
        self.should_stop = False
        self.recv_thread: Optional[threading.Thread] = None
        self.message_callback: Optional[Callable[[str, str], None]] = None
        self.callback_lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.last_send_time = time.time()
        
        # Configure logging
        self.logger = logging.getLogger(f"EasyGameChat-{id(self)}")
    
    def connect(self, nickname: str) -> bool:
        """
        Connect to the chat server with the given nickname.
        
        Args:
            nickname: User's nickname (must pass validation)
            
        Returns:
            True if connection successful, False otherwise
        """
        if self.running:
            return False
        
        # Validate nickname
        if not is_valid_nickname(nickname):
            self.logger.error("Invalid nickname format")
            return False
        
        try:
            # Create socket with timeout
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(CONNECT_TIMEOUT_MS / 1000.0)
            
            # Connect to server
            self.socket.connect((self.host, self.port))
            
            # Set non-blocking for receive operations
            self.socket.settimeout(RECV_TIMEOUT_MS / 1000.0)
            
            self.nickname = nickname
            
            # Send nickname as initial message
            hello_msg = {
                "from": "Client",
                "text": nickname
            }
            
            if not self._send_json(hello_msg):
                self.socket.close()
                self.socket = None
                return False
            
            # Start receive thread
            self.should_stop = False
            self.running = True
            self.recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self.recv_thread.start()
            
            return True
            
        except Exception as e:
            self.logger.error(f"Connection failed: {e}")
            if self.socket:
                self.socket.close()
                self.socket = None
            return False
    
    def send_message(self, text: str) -> bool:
        """
        Send a message to the chat.
        
        Args:
            text: Message text (must pass validation)
            
        Returns:
            True if message sent successfully, False otherwise
        """
        if not self.socket or not self.running:
            return False
        
        # Validate message
        if not is_valid_message(text):
            return False
        
        # Rate limiting
        with self.send_lock:
            now = time.time()
            elapsed_ms = (now - self.last_send_time) * 1000
            
            if elapsed_ms < MIN_SEND_INTERVAL_MS:
                return False  # Rate limited
            
            self.last_send_time = now
        
        # Create and send message
        message = {
            "from": self.nickname,
            "text": text
        }
        
        return self._send_json(message)
    
    def set_message_callback(self, callback: Optional[Callable[[str, str], None]]):
        """
        Set callback function for incoming messages.
        
        Args:
            callback: Function that takes (from_user, text) parameters, or None to clear
        """
        with self.callback_lock:
            self.message_callback = callback
    
    def disconnect(self):
        """Disconnect from the chat server."""
        if self.running:
            self.should_stop = True
            self.running = False
            
            if self.recv_thread and self.recv_thread.is_alive():
                self.recv_thread.join(timeout=1.0)
            
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
    
    def _send_json(self, data: Dict[str, Any]) -> bool:
        """Send JSON data to server with validation."""
        if not self.socket:
            return False
        
        try:
            json_str = json.dumps(data, separators=(',', ':'))
            
            # Validate JSON output
            if not is_secure_json(json_str):
                return False
            
            message = json_str + '\n'
            message_bytes = message.encode('utf-8')
            
            # Send with proper error handling
            total_sent = 0
            retries = 0
            max_retries = 10
            
            while total_sent < len(message_bytes) and retries < max_retries:
                try:
                    sent = self.socket.send(message_bytes[total_sent:])
                    if sent == 0:
                        return False  # Connection closed
                    total_sent += sent
                    retries = 0  # Reset retries on successful send
                except socket.timeout:
                    retries += 1
                    time.sleep(0.01)  # 10ms delay
                    continue
                except Exception:
                    return False
            
            return total_sent == len(message_bytes)
            
        except Exception as e:
            self.logger.error(f"JSON send error: {e}")
            return False
    
    def _recv_loop(self):
        """Main receive loop running in separate thread."""
        buffer = b''
        max_messages_per_loop = 10  # Prevent message flooding
        
        while self.running and not self.should_stop and self.socket:
            try:
                # Receive data with timeout
                data = self.socket.recv(MAX_BUFFER_SIZE)
                
                if not data:
                    break  # Connection closed
                
                # Prevent buffer overflow attacks
                if len(buffer) + len(data) > MAX_BUFFER_SIZE:
                    buffer = b''  # Reset on potential attack
                    continue
                
                buffer += data
                
                # Process complete lines
                messages_processed = 0
                while b'\n' in buffer and messages_processed < max_messages_per_loop:
                    line_bytes, buffer = buffer.split(b'\n', 1)
                    messages_processed += 1
                    
                    try:
                        line = line_bytes.decode('utf-8').strip()
                    except UnicodeDecodeError:
                        continue  # Skip invalid UTF-8
                    
                    if len(line) > MAX_MESSAGE_LENGTH:
                        continue
                    
                    if is_secure_json(line):
                        self._process_message(line)
                        
            except socket.timeout:
                continue  # Normal timeout, check if we should stop
            except Exception as e:
                self.logger.error(f"Receive error: {e}")
                break
    
    def _process_message(self, json_str: str):
        """Process a received JSON message."""
        try:
            data = json.loads(json_str)
            
            # Strict validation
            if (isinstance(data, dict) and 
                'from' in data and 'text' in data and
                isinstance(data['from'], str) and 
                isinstance(data['text'], str) and
                len(data) == 2):  # Only allow exactly these two fields
                
                from_user = data['from']
                text = data['text']
                
                # Double-validate with our secure functions
                """ like in c++, i have removed temporarily is_valid_nickname(from_user) and """ 
                if is_valid_message(text): 
                    with self.callback_lock:
                        if self.message_callback:
                            try:
                                self.message_callback(from_user, text)
                            except Exception as e:
                                self.logger.error(f"Callback error: {e}")
                                
        except json.JSONDecodeError:
            pass  # Ignore malformed JSON

# Simple C-style API for compatibility
_clients: Dict[int, EasyGameChat] = {}
_client_counter = 0
_clients_lock = threading.Lock()

def egc_create(host: str, port: int) -> Optional[int]:
    """
    Create a new EasyGameChat client.
    
    Args:
        host: Server hostname or IP
        port: Server port
        
    Returns:
        Client handle (integer) or None on failure
    """
    global _client_counter
    
    if not host or port <= 0 or port > 65535:
        return None
    
    try:
        client = EasyGameChat(host, port)
        with _clients_lock:
            handle = _client_counter
            _client_counter += 1
            _clients[handle] = client
            return handle
    except Exception:
        return None

def egc_connect(handle: int, nickname: str) -> bool:
    """Connect client to server with nickname."""
    with _clients_lock:
        client = _clients.get(handle)
        if not client:
            return False
        return client.connect(nickname)

def egc_send(handle: int, text: str) -> bool:
    """Send message through client."""
    with _clients_lock:
        client = _clients.get(handle)
        if not client:
            return False
        return client.send_message(text)

def egc_set_message_callback(handle: int, callback: Optional[Callable[[str, str], None]]):
    """Set message callback for client."""
    with _clients_lock:
        client = _clients.get(handle)
        if client:
            client.set_message_callback(callback)

def egc_destroy(handle: int):
    """Destroy client and free resources."""
    with _clients_lock:
        client = _clients.pop(handle, None)
        if client:
            client.disconnect()

# Example usage
if __name__ == "__main__":
    import sys
    
    def on_message(from_user: str, text: str):
        print(f"[{from_user}]: {text}")
    
    # Get nickname from user
    nickname = input("Insert your nickname: ").strip()
    
    # Create and connect client
    client = EasyGameChat("127.0.0.1", 3000)
    client.set_message_callback(on_message)
    
    if not client.connect(nickname):
        print("Error: could not connect to the server")
        sys.exit(1)
    
    print("Connected! Write messages and press ENTER to send them (write '/exit' to exit)")
    
    try:
        while True:
            message = input().strip()
            if message == "/exit":
                break
            if message:  # Don't send empty messages
                if not client.send_message(message):
                    print("Failed to send message (rate limited or invalid)")
    except KeyboardInterrupt:
        pass
    finally:
        client.disconnect()
        print("Disconnected.")