package server

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log/slog"
	"net"
	"strings"
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

	MinJWTSecretLength = 32
)

var (
	clients     = make(map[net.Conn]*Client)
	mutex       sync.RWMutex
	broadcast   = make(chan Message, 1000) // Buffered to prevent blocking
	clientCount int
)

type Config struct {
	Addr        string
	CertFile    string
	KeyFile     string
	DisableAuth bool
	JWTSecret   []byte
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

	if !cfg.DisableAuth && len(cfg.JWTSecret) < MinJWTSecretLength {
		return fmt.Errorf("JWT secret must be at least %d bytes", MinJWTSecretLength)
	}

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
	slog.Info("server listening",
		"addr", cfg.Addr,
		"auth", authMode,
		"cert", cfg.CertFile,
		"max_clients", MaxClients)

	go broadcastLoop()

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
			slog.Error("accept failed", "error", err)
			continue
		}

		mutex.RLock()
		currentClients := clientCount
		mutex.RUnlock()

		if currentClients >= MaxClients {
			slog.Warn("connection limit reached", "remote", conn.RemoteAddr().String())
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

type tokenClaims struct {
	Username string `json:"username"`
	jwt.RegisteredClaims
}

func validateToken(tokenString string, secret []byte) (string, error) {
	claims := &tokenClaims{}
	token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (interface{}, error) {
		if token.Method.Alg() != jwt.SigningMethodHS256.Alg() {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return secret, nil
	}, jwt.WithValidMethods([]string{jwt.SigningMethodHS256.Alg()}))
	if err != nil {
		return "", err
	}
	if !token.Valid {
		return "", fmt.Errorf("invalid token")
	}
	if claims.ExpiresAt == nil {
		return "", fmt.Errorf("token has no expiration")
	}
	if time.Now().After(claims.ExpiresAt.Time) {
		return "", fmt.Errorf("token has expired")
	}
	if claims.Username == "" {
		return "", fmt.Errorf("token has no username")
	}
	return claims.Username, nil
}

func writeError(conn net.Conn, message string) {
	response := map[string]string{"status": "error", "message": message}
	responseBytes, _ := json.Marshal(response)
	conn.Write(responseBytes)
	conn.Close()
}

func handleConnection(conn net.Conn, cfg Config) {
	conn.SetReadDeadline(time.Now().Add(ConnectionTimeout))

	buffer := make([]byte, 1024)
	n, err := conn.Read(buffer)
	if err != nil {
		slog.Debug("handshake read failed", "remote", conn.RemoteAddr().String(), "error", err)
		conn.Close()
		return
	}

	var initialMessage map[string]string
	err = json.Unmarshal(buffer[:n], &initialMessage)
	if err != nil || initialMessage["text"] == "" {
		slog.Warn("invalid handshake", "remote", conn.RemoteAddr().String())
		conn.Close()
		return
	}

	nickname := initialMessage["text"]

	if !cfg.DisableAuth {
		tokenUsername, err := validateToken(initialMessage["token"], cfg.JWTSecret)
		if err != nil || !strings.EqualFold(tokenUsername, nickname) {
			slog.Warn("authentication failed", "remote", conn.RemoteAddr().String())
			writeError(conn, "Invalid token")
			return
		}
	}

	if !isValidNickname(nickname) || isNicknameTaken(nickname) {
		slog.Warn("nickname unavailable", "nickname", nickname, "remote", conn.RemoteAddr().String())
		writeError(conn, "Nickname unavailable")
		return
	}

	response := map[string]string{"status": "success"}
	responseBytes, _ := json.Marshal(response)
	if _, err := conn.Write(responseBytes); err != nil {
		slog.Debug("handshake write failed", "remote", conn.RemoteAddr().String(), "error", err)
		conn.Close()
		return
	}

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
	count := clientCount
	mutex.Unlock()

	slog.Info("client joined", "nickname", c.nickname, "clients", count)

	msg := Message{From: "Server", Text: c.nickname + " joined the chatroom"}
	select {
	case broadcast <- msg:
	default:
		slog.Warn("broadcast queue full", "event", "join", "nickname", c.nickname)
	}
}

func UnregisterClient(c *Client) {
	mutex.Lock()
	if _, exists := clients[c.conn]; exists {
		delete(clients, c.conn)
		clientCount--
	}
	count := clientCount
	mutex.Unlock()

	c.conn.Close()
	close(c.ch)

	slog.Info("client left", "nickname", c.nickname, "clients", count)

	msg := Message{From: "Server", Text: c.nickname + " left the chatroom"}
	select {
	case broadcast <- msg:
	default:
		slog.Warn("broadcast queue full", "event", "leave", "nickname", c.nickname)
	}
}

// GenerateToken creates a signed token for username.
func GenerateToken(username string, secret []byte, ttl time.Duration) (string, error) {
	if !isValidNickname(username) {
		return "", fmt.Errorf("invalid username")
	}
	if len(secret) < MinJWTSecretLength {
		return "", fmt.Errorf("JWT secret must be at least %d bytes", MinJWTSecretLength)
	}

	now := time.Now()
	claims := tokenClaims{
		Username: username,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(now.Add(ttl)),
			IssuedAt:  jwt.NewNumericDate(now),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString(secret)
}
