"""
EasyGameChat Python Client Library
A secure, feature-complete port of the C++ EasyGameChat library
"""

import socket
import ssl
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
    
    def __init__(self, host: str, port: int, use_tls: bool = True,
                 verify_cert: bool = True, ca_cert_path: Optional[str] = None,
                 client_cert_path: Optional[str] = None, client_key_path: Optional[str] = None):
        """
        Initialize EasyGameChat client.
        
        Args:
            host: Server hostname or IP address
            port: Server port number
            use_tls: Enable TLS encryption (default: True)
            verify_cert: Verify server certificate (default: True)
            ca_cert_path: Path to CA certificate file (optional)
            client_cert_path: Path to client certificate for mutual TLS (optional)
            client_key_path: Path to client private key for mutual TLS (optional)
        """
        self.host = host
        self.port = port
        self.use_tls = use_tls
        self.verify_cert = verify_cert
        self.ca_cert_path = ca_cert_path
        self.client_cert_path = client_cert_path
        self.client_key_path = client_key_path

        self.socket: Optional[socket.socket] = None
        self.tls_socket: Optional[ssl.SSLSocket] = None
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

        # Setup TLS context if needed
        self.ssl_context = None
        if self.use_tls:
            self._setup_ssl_context()
    
    def _setup_ssl_context(self):
        """Setup SSL context with security configurations"""
        try:
            self.ssl_context = ssl.create_default_context()

            if not self.verify_cert:
                self.ssl_context.check_hostname = False
                self.ssl_context.verify_mode = ssl.CERT_NONE
                self.logger.warning("Certificate verification disabled - use only for testing!")
            else:
                self.ssl_context.verify_mode = ssl.CERT_REQUIRED
                self.ssl_context.check_hostname = True
            
            if self.ca_cert_path:
                self.ssl_context.load_verify_locations(self.ca_cert_path)
            
            if self.client_cert_path and self.client_key_path:
                self.ssl_context.load_cert_chain(self.client_cert_path, self.client_key_path)

            self.ssl_context.set_ciphers('ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20:!aNULL:!MD5:!DSS')
            self.ssl_context.minimum_version = ssl.TLSVersion.TLSv1_2

            self.ssl_context.options |= ssl.OP_NO_SSLv2
            self.ssl_context.options |= ssl.OP_NO_SSLv3
            self.ssl_context.options |= ssl.OP_NO_TLSv1
            self.ssl_context.options |= ssl.OP_NO_TLSv1_1
            self.ssl_context.options |= ssl.OP_SINGLE_DH_USE
            self.ssl_context.options |= ssl.OP_SINGLE_ECDH_USE

        except Exception as e:
            self.logger.error(f"Failed to setup SSL context: {e}")
            raise TLSError(f"SSL context setup failed: {e}")
    
    def connect(self, nickname: str, token: str) -> bool:
        """
        Connect to the chat server with the given nickname and token using TLS if enabled.
        
        Args:
            nickname: User's nickname (must pass validation)
            token: Authentication token
            
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

            # Setup TLS if enabled
            if self.use_tls:
                if not self.ssl_context:
                    self.logger.error("SSL context not initialized")
                    self.socket.close()
                    return False
                
                try:
                    # Wrap socket with TLS
                    self.tls_socket = self.ssl_context.wrap_socket(
                        self.socket,
                        server_hostname=self.host if self.verify_cert else None
                    )

                    # Perform TLS handshake
                    self.tls_socket.do_handshake()

                    # Log TLS connection info
                    cipher = self.tls_socket.cipher()
                    if cipher:
                        self.logger.info(f"TLS connection established: {cipher[0]} {cipher[1]}")

                    # Get peer certificate info
                    if self.verify_cert:
                        cert = self.tls_socket.getpeercert()
                        if cert:
                            subject = dict(x[0] for x in cert['subject'])
                            self.logger.info(f"Server certificate: {subject.get('commonName', 'Unknown')}")
                    
                    # Use TLS socket for communication
                    active_socket = self.tls_socket

                except ssl.SSLError as e:
                    self.logger.error(f"TLS handshake failed: {e}")
                    self.socket.close()
                    self.socket = None
                    return False
            
            else:
                # Use plain socket
                active_socket = self.socket
            
            # Set non-blocking for receive operations
            active_socket.settimeout(RECV_TIMEOUT_MS / 1000.0)
            
            self.nickname = nickname
            
            # Send nickname and token as initial message
            hello_msg = {
                "from": "Client",
                "text": nickname,
                "token": token
            }

            self.logger.debug(f"Sending initial message: {hello_msg}")

            if not self._send_json(hello_msg):
                self._close_connection()
                return False

            # Wait for server response after sending the token
            try:
                response = active_socket.recv(1024).decode('utf-8')
                self.logger.debug(f"Received server response: {response}")
                response_data = json.loads(response)
                if response_data.get("status") != "success":
                    self.logger.error(f"Server rejected connection: {response_data.get('message', 'Unknown error')}")
                    self._close_connection()
                    return False
            except json.JSONDecodeError:
                self.logger.error("Invalid response from server")
                self._close_connection()
                return False
            
            # Start receive thread
            self.should_stop = False
            self.running = True
            self.recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self.recv_thread.start()
            
            return True
            
        except Exception as e:
            self.logger.error(f"Connection failed: {e}")
            self._close_connection()
            return False
    
    def send_message(self, text: str) -> bool:
        """
        Send a message to the chat.
        
        Args:
            text: Message text (must pass validation)
            
        Returns:
            True if message sent successfully, False otherwise
        """
        if not self._get_active_socket() or not self.running:
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
            
            self._close_connection()

    def get_connection_info(self) -> Dict[str, Any]:
        """
        Get information about the current connection.
        
        Returns:
            Dictionary with connection details including TLS info
        """

        info = {
            "connected": self.running,
            "host": self.host,
            "port": self.port,
            "nickname": self.nickname,
            "tls_enabled": self.use_tls,
        }

        if self.use_tls and self.tls_socket:
            try:
                cipher = self.tls_socket.cipher()
                if cipher:
                    info["tls_cipher"] = cipher[0]
                    info["tls_version"] = cipher[1]
                    info["tls_bits"] = cipher[2]
                
                if self.verify_cert:
                    cert = self.tls_socket.getpeercert()
                    if cert:
                        subject = dict(x[0] for x in cert['subject'])
                        info["server_cert_cn"] = subject.get('commonName', 'Unknown')
                        info["server_cert_valid"] = True
            except Exception as e:
                info["tls_error"] = str(e)
        
        return info
    
    def _get_active_socket(self) -> Optional[socket.socket]:
        """Get the active socket (TLS or plain)"""
        if self.use_tls:
            return self.tls_socket
        return self.socket
    
    def _close_connection(self):
        """Close all socket connections"""
        if self.tls_socket:
            try:
                self.tls_socket.unwrap()
            except:
                pass
            try:
                self.tls_socket.close()
            except:
                pass
            self.tls_socket = None
        
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None

    
    def _send_json(self, data: Dict[str, Any]) -> bool:
        """Send JSON data to server with validation."""
        active_socket = self._get_active_socket()

        if not active_socket:
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
                    sent = active_socket.send(message_bytes[total_sent:])
                    if sent == 0:
                        return False  # Connection closed
                    total_sent += sent
                    retries = 0  # Reset retries on successful send
                except socket.timeout:
                    retries += 1
                    time.sleep(0.01)  # 10ms delay
                    continue
                except (ssl.SSLError, socket.error) as e:
                    self.logger.error(f"Send error: {e}")
                    return False
            
            return total_sent == len(message_bytes)
            
        except Exception as e:
            self.logger.error(f"JSON send error: {e}")
            return False
    
    def _recv_loop(self):
        """Main receive loop running in separate thread."""
        buffer = b''
        max_messages_per_loop = 10  # Prevent message flooding
        active_socket = self._get_active_socket()
        
        while self.running and not self.should_stop and self.socket:
            try:
                # Receive data with timeout
                data = active_socket.recv(MAX_BUFFER_SIZE)
                
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
            except (ssl.SSLError, socket.error) as e:
                if not self.should_stop:  # Only log if not intentionally stopping
                    self.logger.error(f"Receive error: {e}")
                break
            except Exception as e:
                self.logger.error(f"Unexpected receive error: {e}")
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

def egc_create(host: str, port: int, use_tls: bool = True) -> Optional[int]:
    """
    Create a new EasyGameChat client with TLS support.
    
    Args:
        host: Server hostname or IP
        port: Server port
        use_tls: Enable TLS encryption (default: True)
        
    Returns:
        Client handle (integer) or None on failure
    """
    global _client_counter
    
    if not host or port <= 0 or port > 65535:
        return None
    
    try:
        client = EasyGameChat(host, port, use_tls=use_tls)
        with _clients_lock:
            handle = _client_counter
            _client_counter += 1
            _clients[handle] = client
            return handle
    except Exception:
        return None
    
def egc_create_with_certs(host: str, port: int, verify_cert: bool = True, 
                         ca_cert_path: Optional[str] = None,
                         client_cert_path: Optional[str] = None, 
                         client_key_path: Optional[str] = None) -> Optional[int]:
    """
    Create a new EasyGameChat client with custom TLS certificate configuration.
    
    Args:
        host: Server hostname or IP
        port: Server port
        verify_cert: Verify server certificate
        ca_cert_path: Path to CA certificate file
        client_cert_path: Path to client certificate for mutual TLS
        client_key_path: Path to client private key for mutual TLS
        
    Returns:
        Client handle (integer) or None on failure
    """
    global _client_counter
    
    if not host or port <= 0 or port > 65535:
        return None
    
    try:
        client = EasyGameChat(host, port, use_tls=True, verify_cert=verify_cert,
                             ca_cert_path=ca_cert_path, client_cert_path=client_cert_path,
                             client_key_path=client_key_path)
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