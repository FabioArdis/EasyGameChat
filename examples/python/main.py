#!/usr/bin/env python3
"""
EasyGameChat Python Example with TLS Support
"""
import sys
import time
from pathlib import Path
# I need to remember to delete this once i put the package on pip
#path_root = Path(__file__).parents[2]
#sys.path.append(str(path_root))
#print(sys.path)

#from clients.python.easygamechat import EasyGameChat
# Remember to install the library before importing it
from easygamechat import EasyGameChat

def main():
    # Get nickname from user
    nickname = input("Insert your nickname: ").strip()
    
    # Get TLS configuration from user
    print("\nTLS Configuration:")
    use_tls_input = input("Use TLS encryption? (y/n) [y]: ").strip().lower()
    use_tls = use_tls_input != 'n'
    
    # Get server details
    print("\nServer Configuration:")
    default_port = 3000
    host = input(f"Server host [127.0.0.1]: ").strip() or "127.0.0.1"
    port_input = input(f"Server port [{default_port}]: ").strip()
    port = int(port_input) if port_input else default_port
    
    # Create client with TLS configuration
    if use_tls:
        # For development/testing, allow skipping certificate verification
        if host in ['127.0.0.1', 'localhost']:
            verify_cert = input("Verify server certificate? (y/n) [n]: ").strip().lower() == 'y'
        else:
            verify_cert = input("Verify server certificate? (y/n) [y]: ").strip().lower() != 'n'
        
        # Ask about custom certificates
        use_custom_certs = input("Use custom certificates? (y/n) [n]: ").strip().lower() == 'y'
        
        if use_custom_certs:
            ca_cert_path = input("CA certificate path (optional): ").strip() or None
            client_cert_path = input("Client certificate path (optional): ").strip() or None
            client_key_path = input("Client key path (optional): ").strip() or None
            
            client = EasyGameChat(host, port, use_tls=True, verify_cert=verify_cert,
                                 ca_cert_path=ca_cert_path, 
                                 client_cert_path=client_cert_path,
                                 client_key_path=client_key_path)
        else:
            client = EasyGameChat(host, port, use_tls=True, verify_cert=verify_cert)
    else:
        client = EasyGameChat(host, port, use_tls=False)
    
    # Set up message callback
    def on_message(from_user: str, text: str):
        print(f"[{from_user}]: {text}")
    
    client.set_message_callback(on_message)
    
    # Connect to server
    print(f"\nConnecting to {host}:{port} {'with TLS' if use_tls else 'without TLS'}...")
    
    if not client.connect(nickname):
        print("Error: could not connect to the server")
        return 1
    
    # Show connection information
    conn_info = client.get_connection_info()
    print(f"✓ Connected successfully!")
    print(f"  Host: {conn_info['host']}:{conn_info['port']}")
    print(f"  Nickname: {conn_info['nickname']}")
    print(f"  TLS Enabled: {conn_info['tls_enabled']}")
    
    if use_tls and conn_info.get('tls_cipher'):
        print(f"  TLS Cipher: {conn_info['tls_cipher']}")
        print(f"  TLS Version: {conn_info['tls_version']}")
        if conn_info.get('server_cert_cn'):
            print(f"  Server Certificate: {conn_info['server_cert_cn']}")
    
    print("\nCommands:")
    print("  Type messages and press ENTER to send")
    print("  '/exit' - Exit the chat")
    print("  '/info' - Show connection information")
    print("  '/help' - Show this help message")
    print()
    
    try:
        while True:
            message = input().strip()
            if message == "/exit":
                break
            elif message == "/info":
                # Show detailed connection information
                info = client.get_connection_info()
                print("\n--- Connection Information ---")
                for key, value in info.items():
                    print(f"  {key}: {value}")
                print("--- End Connection Info ---\n")
            elif message == "/help":
                print("\n--- Available Commands ---")
                print("  /exit - Exit the chat")
                print("  /info - Show connection information")
                print("  /help - Show this help message")
                print("--- End Help ---\n")
            elif message:  # Don't send empty messages
                if not client.send_message(message):
                    print("Warning: Failed to send message (might be rate limited or invalid)")
            # Empty messages are ignored
    
    except KeyboardInterrupt:
        print("\nInterrupted by user")
    except EOFError:
        print("\nEnd of input")
    except Exception as e:
        print(f"\nUnexpected error: {e}")
    finally:
        client.disconnect()
        print("Disconnected.")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())