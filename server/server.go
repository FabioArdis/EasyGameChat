package server

import (
	"crypto/sha256"
	"crypto/tls"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"sync"
	"time"

	"github.com/golang-jwt/jwt/v4"
)

const (
	MaxClients        = 100
	ConnectionTimeout = 30 * time.Second
	ServerPort        = ":3000"

	// TLS Configuration
	CertFile  = "certs/server.crt"
	KeyFile   = "certs/server.key"
	tokenFile = "tokens.json"
)

var (
	clients     = make(map[net.Conn]*Client)
	mutex       sync.RWMutex
	broadcast   = make(chan Message, 1000) // Buffered to prevent blocking
	clientCount int

	// Secret key for JWT validation (STORE THIS SECURELY IF YOU USE THIS IN PRODUCTION)
	jwtSecretKey = []byte("super-duper-secret-key") // Change this to a secure key in production
)

func StartTLS() {
	// Load TLS certificate and key
	cert, err := tls.LoadX509KeyPair(CertFile, KeyFile)

	if err != nil {
		panic(fmt.Sprintf("Failed to load TLS certificate: %v", err))
	}

	// Configure TLS
	tlsConfig := &tls.Config{
		Certificates: []tls.Certificate{cert},
		MinVersion:   tls.VersionTLS12, // Minimum TLS 1.2
		CipherSuites: []uint16{
			tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,
			tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		},
	}

	// Create TLS listener
	ln, err := tls.Listen("tcp", ServerPort, tlsConfig)

	if err != nil {
		panic(fmt.Sprintf("Failed to start TLS listener: %v", err))
	}
	defer ln.Close()

	fmt.Printf("[SERVER] TLS Server listening on port %s...\n", ServerPort)
	fmt.Printf("[SERVER] Max clients: %d\n", MaxClients)
	fmt.Printf("[SERVER] Using TLS certificate: %s\n", CertFile)

	go debugBroadcaster()

	for {
		conn, err := ln.Accept()

		if err != nil {
			fmt.Println("[ERROR] TLS Connection refused:", err)
			continue
		}

		// Check connection limits
		mutex.RLock()
		currentClients := clientCount
		mutex.RUnlock()

		if currentClients >= MaxClients {
			fmt.Printf("[WARN] Connection limit reached, rejecting %s\n", conn.RemoteAddr())
		}

		go handleConnection(conn)
	}
}

// Old Start (we may keep this for compatibility or further developmnet)

func Start() {
	ln, err := net.Listen("tcp", ServerPort)
	if err != nil {
		panic(err)
	}
	defer ln.Close()

	fmt.Printf("[WARNING] USING TLS-LESS VERSION\n")

	fmt.Printf("[SERVER] Listening on port %s...\n", ServerPort)
	fmt.Printf("[SERVER] Max clients: %d\n", MaxClients)

	go debugBroadcaster()

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println("[ERROR] Connection refused:", err)
			continue
		}

		// Check connection limits
		mutex.RLock()
		currentClients := clientCount
		mutex.RUnlock()

		if currentClients >= MaxClients {
			fmt.Printf("[WARN] Connection limit reached, rejecting %s\n", conn.RemoteAddr())
			conn.Close()
			continue
		}

		go handleConnection(conn)
	}
}

// Add token validation function
func validateToken(token string) bool {
	// Parse and validate the JWT token
	tokenObj, err := jwt.Parse(token, func(token *jwt.Token) (interface{}, error) {
		// Ensure the signing method is HMAC
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return jwtSecretKey, nil
	})

	if err != nil {
		fmt.Println("[ERROR] Token validation failed:", err)
		return false
	}

	// Check if the token is valid
	if claims, ok := tokenObj.Claims.(jwt.MapClaims); ok && tokenObj.Valid {
		// Check token expiration
		if exp, ok := claims["exp"].(float64); ok {
			expirationTime := time.Unix(int64(exp), 0)
			if time.Now().After(expirationTime) {
				fmt.Println("[ERROR] Token has expired")
				return false
			}
		}
		return true
	}

	return false
}

func handleConnection(conn net.Conn) {
	// Set connection timeout only for initial handshake
	conn.SetReadDeadline(time.Now().Add(ConnectionTimeout))

	// Read initial message for token validation
	buffer := make([]byte, 1024)
	n, err := conn.Read(buffer)
	if err != nil {
		fmt.Println("[ERROR] Failed to read from connection:", err)
		conn.Close()
		return
	}

	// Log the received message for debugging
	fmt.Printf("[DEBUG] Received initial message: %s\n", string(buffer[:n]))

	// Parse token and nickname from the message
	var initialMessage map[string]string
	err = json.Unmarshal(buffer[:n], &initialMessage)
	if err != nil || initialMessage["token"] == "" || initialMessage["text"] == "" {
		fmt.Println("[ERROR] Invalid initial message format or missing token/nickname")
		conn.Close()
		return
	}

	nickname := initialMessage["text"]

	// Validate the token
	fmt.Printf("[DEBUG] Validating token: %s\n", initialMessage["token"])
	if !validateToken(initialMessage["token"]) {
		fmt.Println("[ERROR] Invalid token")
		response := map[string]string{"status": "error", "message": "Invalid token"}
		responseBytes, _ := json.Marshal(response)
		conn.Write(responseBytes)
		conn.Close()
		return
	}

	// Send success response
	response := map[string]string{"status": "success"}
	responseBytes, _ := json.Marshal(response)
	conn.Write(responseBytes)

	// Create and register the client using the nickname from the initial message
	client := NewClient(conn, nickname)
	if client == nil {
		return
	}

	RegisterClient(client)
	defer UnregisterClient(client)

	fmt.Printf("[INFO] %s joined (clients: %d)\n", client.nickname, clientCount)

	// Client handles clearing timeouts in Listen()
	client.Listen()
}

func RegisterClient(c *Client) {
	mutex.Lock()
	clients[c.conn] = c
	clientCount++
	mutex.Unlock()

	fmt.Printf("[INFO] %s joined (clients: %d)\n", c.nickname, clientCount)

	msg := Message{From: "Server", Text: c.nickname + " joined the chatroom"}
	select {
	case broadcast <- msg:
	default:
		fmt.Println("[WARN] Broadcast channel full, dropping join message")
	}
}

func UnregisterClient(c *Client) {
	mutex.Lock()
	if _, exists := clients[c.conn]; exists {
		delete(clients, c.conn)
		clientCount--
	}
	mutex.Unlock()

	c.conn.Close()
	close(c.ch)

	fmt.Printf("[INFO] %s left (clients: %d)\n", c.nickname, clientCount)

	msg := Message{From: "Server", Text: c.nickname + " left the chatroom"}
	select {
	case broadcast <- msg:
	default:
		fmt.Println("[WARN] Broadcast channel full, dropping leave message")
	}
}

func broadcaster() {
	for msg := range broadcast {
		mutex.RLock()
		for _, c := range clients {
			if c.nickname != msg.From {
				select {
				case c.ch <- msg:
				default:
					// Client's channel is full, skip this message
					fmt.Printf("[WARN] Message dropped for %s (channel full)\n", c.nickname)
				}
			}
		}
		mutex.RUnlock()
	}
}

// GenerateToken creates a new token and stores it securely
func GenerateToken(username string) (string, error) {
	// Create a new token
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"username": username,
		"exp":      time.Now().Add(24 * time.Hour).Unix(),
	})

	signedToken, err := token.SignedString(jwtSecretKey)
	if err != nil {
		return "", err
	}

	// Hash the token for secure storage
	hash := sha256.Sum256([]byte(signedToken))
	hashedToken := hex.EncodeToString(hash[:])

	// Store the hashed token in a JSON file
	tokens := make(map[string]string)
	if _, err := os.Stat(tokenFile); err == nil {
		data, err := os.ReadFile(tokenFile)
		if err == nil {
			json.Unmarshal(data, &tokens)
		}
	}

	tokens[username] = hashedToken
	data, err := json.Marshal(tokens)
	if err != nil {
		return "", err
	}

	err = os.WriteFile(tokenFile, data, 0644)
	if err != nil {
		return "", err
	}

	return signedToken, nil
}
