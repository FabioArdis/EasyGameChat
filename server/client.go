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
	fmt.Printf("[CLIENT] New client connection from %s\n", conn.RemoteAddr())

	// Set timeout for initial handshake
	conn.SetReadDeadline(time.Now().Add(time.Duration(ReadTimeoutMs) * time.Millisecond))

	decoder := json.NewDecoder(conn)
	var hello Message

	fmt.Printf("[CLIENT] Waiting for nickname from %s\n", conn.RemoteAddr())
	if err := decoder.Decode(&hello); err != nil {
		fmt.Printf("[ERROR] Failed to read nickname from %s: %v\n", conn.RemoteAddr(), err)
		conn.Close()
		return nil
	}

	fmt.Printf("[CLIENT] Received nickname data: From='%s', Text='%s'\n", hello.From, hello.Text)

	// Sanitize and validate nickname
	nickname := sanitizeString(hello.Text)
	fmt.Printf("[CLIENT] Sanitized nickname: '%s' -> '%s'\n", hello.Text, nickname)

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

	fmt.Printf("[CLIENT] Successfully created client '%s' from %s\n", nickname, conn.RemoteAddr())
	go client.writeLoop()
	return client
}

func (c *Client) Listen() {
	fmt.Printf("[LISTEN] Starting listen loop for client '%s'\n", c.nickname)

	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("[ERROR] Client %s panic: %v\n", c.nickname, r)
		}
		fmt.Printf("[LISTEN] Listen loop ended for client '%s'\n", c.nickname)
	}()

	// Clear any deadlines set during handshake - we don't want to timeout idle users
	c.conn.SetReadDeadline(time.Time{})

	reader := bufio.NewReader(c.conn)
	messageCount := 0

	for {
		fmt.Printf("[LISTEN] %s: Waiting for message...\n", c.nickname)

		line, err := reader.ReadString('\n')
		if err != nil {
			fmt.Printf("[DEBUG] ReadString error for %s: %v\n", c.nickname, err)
			break
		}

		fmt.Printf("[LISTEN] %s: Raw received: %q (length: %d)\n", c.nickname, line, len(line))

		line = strings.TrimSpace(line)
		if line == "" {
			fmt.Printf("[LISTEN] %s: Empty line received, continuing\n", c.nickname)
			continue
		}

		fmt.Printf("[LISTEN] %s: Trimmed message: %q\n", c.nickname, line)

		// Prevent message flooding
		messageCount++
		if messageCount > MaxMessagesPerLoop {
			fmt.Printf("[WARN] Message rate limit exceeded for %s\n", c.nickname)
			time.Sleep(time.Duration(MinSendIntervalMs) * time.Millisecond)
			messageCount = 0
		}

		// Validate JSON structure before parsing
		fmt.Printf("[LISTEN] %s: Validating JSON structure...\n", c.nickname)
		if !isSecureJsonString(line) {
			fmt.Printf("[ERROR] Invalid JSON structure from %s: %q\n", c.nickname, line)
			continue
		}

		fmt.Printf("[LISTEN] %s: JSON structure valid, unmarshaling...\n", c.nickname)
		var incoming Message
		if err := json.Unmarshal([]byte(line), &incoming); err != nil {
			fmt.Printf("[ERROR] Invalid message JSON from %s: %v (data: %q)\n", c.nickname, err, line)
			continue
		}

		fmt.Printf("[LISTEN] %s: Successfully unmarshaled: From='%s', Text='%s'\n", c.nickname, incoming.From, incoming.Text)

		// Rate limiting check
		now := time.Now()
		timeSinceLastSend := now.Sub(c.lastSend)
		fmt.Printf("[LISTEN] %s: Time since last send: %v (min interval: %v)\n", c.nickname, timeSinceLastSend, time.Duration(MinSendIntervalMs)*time.Millisecond)

		if timeSinceLastSend < time.Duration(MinSendIntervalMs)*time.Millisecond {
			fmt.Printf("[WARN] Rate limit exceeded for %s (waited %v, need %v)\n", c.nickname, timeSinceLastSend, time.Duration(MinSendIntervalMs)*time.Millisecond)
			continue
		}
		c.lastSend = now

		// Validate message content
		fmt.Printf("[LISTEN] %s: Sanitizing message text: %q\n", c.nickname, incoming.Text)
		sanitizedText := sanitizeString(incoming.Text)
		fmt.Printf("[LISTEN] %s: Sanitized text: %q\n", c.nickname, sanitizedText)

		if !isValidMessage(sanitizedText) {
			fmt.Printf("[ERROR] Invalid message content from %s: '%s' (sanitized: '%s')\n", c.nickname, incoming.Text, sanitizedText)
			continue
		}

		// Create message with validated content
		msg := Message{From: c.nickname, Text: sanitizedText}
		fmt.Printf("[LISTEN] %s: Created validated message: %s\n", c.nickname, msg.String())

		// Non-blocking send to broadcast channel
		fmt.Printf("[LISTEN] %s: Attempting to send to broadcast channel...\n", c.nickname)
		select {
		case broadcast <- msg:
			fmt.Printf("[SUCCESS] %s: Message sent to broadcast channel: %s\n", c.nickname, msg.String())
		default:
			fmt.Printf("[WARN] Broadcast channel full, dropping message from %s: %s\n", c.nickname, msg.String())
		}
	}
}

func (c *Client) Send(msg Message) {
	select {
	case c.ch <- msg:
		fmt.Printf("[SEND] Message queued for %s: %s\n", c.nickname, msg.String())
	default:
		// Channel is full, client is probably slow/disconnected
		fmt.Printf("[WARN] Dropping message for %s (channel full): %s\n", c.nickname, msg.String())
	}
}

func (c *Client) writeLoop() {
	fmt.Printf("[WRITE] Starting write loop for client '%s'\n", c.nickname)

	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("[ERROR] WriteLoop panic for %s: %v\n", c.nickname, r)
		}
		fmt.Printf("[WRITE] Write loop ended for client '%s'\n", c.nickname)
	}()

	encoder := json.NewEncoder(c.conn)

	for msg := range c.ch {
		fmt.Printf("[WRITE] %s: Sending message: %s\n", c.nickname, msg.String())

		// Only set write timeout for actual writes, not reads
		c.conn.SetWriteDeadline(time.Now().Add(time.Duration(WriteTimeoutMs) * time.Millisecond))

		if err := encoder.Encode(msg); err != nil {
			fmt.Printf("[ERROR] Could not send message to %s: %v\n", c.nickname, err)
			break
		}

		fmt.Printf("[WRITE] %s: Message sent successfully\n", c.nickname)
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

// Ad hoc debug broadcaster, we'll use this one lest we lose our minds
func debugBroadcaster() {
	fmt.Println("[BROADCASTER] Starting message broadcaster with debug logging")

	for msg := range broadcast {
		fmt.Printf("[BROADCASTER] Received message for broadcasting: From='%s', Text='%s'\n", msg.From, msg.Text)

		mutex.RLock()
		clientCount := len(clients)
		fmt.Printf("[BROADCASTER] Current client count: %d\n", clientCount)

		if clientCount == 0 {
			fmt.Printf("[BROADCASTER] No clients connected, message dropped\n")
			mutex.RUnlock()
			continue
		}

		sentCount := 0
		skippedCount := 0
		droppedCount := 0

		for _, c := range clients {
			fmt.Printf("[BROADCASTER] Processing client '%s' (sender: '%s')\n", c.nickname, msg.From)

			if c.nickname != msg.From {
				select {
				case c.ch <- msg:
					sentCount++
					fmt.Printf("[BROADCASTER] Message sent to '%s'\n", c.nickname)
				default:
					droppedCount++
					fmt.Printf("[BROADCASTER] Message dropped for '%s' (channel full)\n", c.nickname)
				}
			} else {
				skippedCount++
				fmt.Printf("[BROADCASTER] Skipped sender '%s'\n", c.nickname)
			}
		}
		mutex.RUnlock()

		fmt.Printf("[BROADCASTER] Broadcast summary - Sent: %d, Skipped: %d, Dropped: %d\n",
			sentCount, skippedCount, droppedCount)
	}

	fmt.Println("[BROADCASTER] Message broadcaster stopped")
}

// Debug function to test the validation functions
func DebugValidation() {
	fmt.Println("[DEBUG] Testing validation functions...")

	// Test JSON validation
	testCases := []string{
		`{"from":"test","text":"hello"}`,
		`{"from":"test","text":"hello world"}`,
		`{invalid json}`,
		`{"from":"test"}`,
		``,
	}

	for _, test := range testCases {
		valid := isSecureJsonString(test)
		fmt.Printf("[DEBUG] JSON validation for %q: %v\n", test, valid)
	}

	// Test message validation
	messages := []string{
		"Hello world",
		"",
		"   ",
		strings.Repeat("a", 600), // Too long
		"Hello\nworld",           // Control character
	}

	for _, msg := range messages {
		valid := isValidMessage(msg)
		fmt.Printf("[DEBUG] Message validation for %q: %v\n", msg, valid)
	}
}

// Debug function to monitor broadcast channel
func MonitorBroadcastChannel() {
	fmt.Printf("[DEBUG] Broadcast channel capacity: %d\n", cap(broadcast))
	fmt.Printf("[DEBUG] Broadcast channel length: %d\n", len(broadcast))
}
