package server

import (
	"context"
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

	DefaultAddr     = ":3000"
	DefaultCertFile = "certs/server.crt"
	DefaultKeyFile  = "certs/server.key"

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

type Config struct {
	Addr        string
	CertFile    string
	KeyFile     string
	DisableAuth bool
}

func (c Config) withDefaults() Config {
	if c.Addr == "" {
		c.Addr = DefaultAddr
	}
	if c.CertFile == "" {
		c.CertFile = DefaultCertFile
	}
	if c.KeyFile == "" {
		c.KeyFile = DefaultKeyFile
	}
	return c
}

func StartTLS(ctx context.Context, cfg Config) error {
	cfg = cfg.withDefaults()

	cert, err := tls.LoadX509KeyPair(cfg.CertFile, cfg.KeyFile)
	if err != nil {
		return fmt.Errorf("load TLS certificate: %w", err)
	}

	tlsConfig := &tls.Config{
		Certificates: []tls.Certificate{cert},
		MinVersion:   tls.VersionTLS12,
		CipherSuites: []uint16{
			tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,
			tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		},
	}

	ln, err := tls.Listen("tcp", cfg.Addr, tlsConfig)
	if err != nil {
		return fmt.Errorf("listen on %s: %w", cfg.Addr, err)
	}
	defer ln.Close()

	authMode := "token"
	if cfg.DisableAuth {
		authMode = "none"
	}
	fmt.Printf("[SERVER] TLS Server listening on %s...\n", cfg.Addr)
	fmt.Printf("[SERVER] Max clients: %d\n", MaxClients)
	fmt.Printf("[SERVER] Using TLS certificate: %s\n", cfg.CertFile)
	fmt.Printf("[SERVER] Authentication: %s\n", authMode)

	go debugBroadcaster()

	go func() {
		<-ctx.Done()
		ln.Close()
		closeClients()
	}()

	for {
		conn, err := ln.Accept()
		if err != nil {
			if ctx.Err() != nil {
				return nil
			}
			fmt.Println("[ERROR] TLS connection refused:", err)
			continue
		}

		mutex.RLock()
		currentClients := clientCount
		mutex.RUnlock()

		if currentClients >= MaxClients {
			fmt.Printf("[WARN] Connection limit reached, rejecting %s\n", conn.RemoteAddr())
			conn.Close()
			continue
		}

		go handleConnection(conn, cfg)
	}
}

func closeClients() {
	mutex.RLock()
	defer mutex.RUnlock()

	for _, c := range clients {
		c.conn.Close()
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

func handleConnection(conn net.Conn, cfg Config) {
	conn.SetReadDeadline(time.Now().Add(ConnectionTimeout))

	buffer := make([]byte, 1024)
	n, err := conn.Read(buffer)
	if err != nil {
		fmt.Println("[ERROR] Failed to read from connection:", err)
		conn.Close()
		return
	}

	var initialMessage map[string]string
	err = json.Unmarshal(buffer[:n], &initialMessage)
	if err != nil || initialMessage["text"] == "" {
		fmt.Println("[ERROR] Invalid initial message format or missing nickname")
		conn.Close()
		return
	}

	nickname := initialMessage["text"]

	if !cfg.DisableAuth {
		if initialMessage["token"] == "" || !validateToken(initialMessage["token"]) {
			fmt.Println("[ERROR] Invalid token")
			response := map[string]string{"status": "error", "message": "Invalid token"}
			responseBytes, _ := json.Marshal(response)
			conn.Write(responseBytes)
			conn.Close()
			return
		}
	}

	response := map[string]string{"status": "success"}
	responseBytes, _ := json.Marshal(response)
	conn.Write(responseBytes)

	client := NewClient(conn, nickname)
	if client == nil {
		return
	}

	RegisterClient(client)
	defer UnregisterClient(client)

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
