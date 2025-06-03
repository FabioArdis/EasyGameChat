package server

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"strings"
	"time"
)

const (
	MinSendIntervalMs  = 100 // Max 10 messages per second (be sure this matches with your client)
	ReadTimeoutMs      = 5000
	WriteTimeoutMs     = 5000
	MaxMessagesPerLoop = 10 // Prevent message flooding
)

type Client struct {
	conn     net.Conn
	nickname string
	ch       chan Message
	lastSend time.Time
}

func NewClient(conn net.Conn) *Client {
	// Set timeout for initial handshake
	conn.SetReadDeadline(time.Now().Add(time.Duration(ReadTimeoutMs) * time.Millisecond))

	decoder := json.NewDecoder(conn)
	var hello Message

	if err := decoder.Decode(&hello); err != nil {
		fmt.Printf("[ERROR] Failed to read nickname from %s: %v\n", conn.RemoteAddr(), err)
		conn.Close()
		return nil
	}

	// Sanitize and validate nickname
	nickname := sanitizeString(hello.Text)
	if !isValidNickname(nickname) {
		fmt.Printf("[ERROR] Invalid nickname '%s' from %s\n", hello.Text, conn.RemoteAddr())
		conn.Close()
		return nil
	}

	// Check for duplicate nicknames
	if isNicknameTaken(nickname) {
		fmt.Printf("[ERROR] Nickname '%s' already taken from %s\n", nickname, conn.RemoteAddr())
		conn.Close()
		return nil
	}

	client := &Client{
		conn:     conn,
		nickname: nickname,
		ch:       make(chan Message, 100), // Buffered channel to prevent blocking
		lastSend: time.Now(),
	}

	go client.writeLoop()
	return client
}

func (c *Client) Listen() {
	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("[ERROR] Client %s panic: %v\n", c.nickname, r)
		}
	}()

	// Clear any deadlines set during handshake - we don't want to timeout idle users
	c.conn.SetReadDeadline(time.Time{})

	reader := bufio.NewReader(c.conn)
	messageCount := 0

	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			fmt.Printf("[DEBUG] ReadString error for %s: %v\n", c.nickname, err)
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Prevent message flooding
		messageCount++
		if messageCount > MaxMessagesPerLoop {
			fmt.Printf("[WARN] Message rate limit exceeded for %s\n", c.nickname)
			time.Sleep(time.Duration(MinSendIntervalMs) * time.Millisecond)
			messageCount = 0
		}

		// Validate JSON structure before parsing
		if !isSecureJsonString(line) {
			fmt.Printf("[ERROR] Invalid JSON structure from %s\n", c.nickname)
			continue
		}

		var incoming Message
		if err := json.Unmarshal([]byte(line), &incoming); err != nil {
			fmt.Printf("[ERROR] Invalid message JSON from %s: %v\n", c.nickname, err)
			continue
		}

		// Rate limiting check
		now := time.Now()
		if now.Sub(c.lastSend) < time.Duration(MinSendIntervalMs)*time.Millisecond {
			fmt.Printf("[WARN] Rate limit exceeded for %s\n", c.nickname)
			continue
		}
		c.lastSend = now

		// Validate message content
		sanitizedText := sanitizeString(incoming.Text)
		if !isValidMessage(sanitizedText) {
			fmt.Printf("[ERROR] Invalid message content from %s: '%s'\n", c.nickname, incoming.Text)
			continue
		}

		// Create message with validated content
		msg := Message{From: c.nickname, Text: sanitizedText}

		// Non-blocking send to broadcast channel
		select {
		case broadcast <- msg:
		default:
			fmt.Printf("[WARN] Broadcast channel full, dropping message from %s\n", c.nickname)
		}
	}
}

func (c *Client) Send(msg Message) {
	select {
	case c.ch <- msg:
	default:
		// Channel is full, client is probably slow/disconnected
		fmt.Printf("[WARN] Dropping message for %s (channel full)\n", c.nickname)
	}
}

func (c *Client) writeLoop() {
	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("[ERROR] WriteLoop panic for %s: %v\n", c.nickname, r)
		}
	}()

	encoder := json.NewEncoder(c.conn)

	for msg := range c.ch {
		// Only set write timeout for actual writes, not reads
		c.conn.SetWriteDeadline(time.Now().Add(time.Duration(WriteTimeoutMs) * time.Millisecond))

		if err := encoder.Encode(msg); err != nil {
			fmt.Printf("[ERROR] Could not send message to %s: %v\n", c.nickname, err)
			break
		}
	}
}

func isNicknameTaken(nickname string) bool {
	mutex.RLock()
	defer mutex.RUnlock()

	for _, client := range clients {
		if strings.EqualFold(client.nickname, nickname) {
			return true
		}
	}
	return false
}
