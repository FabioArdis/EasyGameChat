package server

import (
	"bufio"
	"encoding/json"
	"log/slog"
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

func NewClient(conn net.Conn, nickname string) *Client {
	if !isValidNickname(nickname) {
		slog.Warn("invalid nickname", "nickname", nickname, "remote", conn.RemoteAddr().String())
		conn.Close()
		return nil
	}

	if isNicknameTaken(nickname) {
		slog.Warn("nickname taken", "nickname", nickname, "remote", conn.RemoteAddr().String())
		conn.Close()
		return nil
	}

	client := &Client{
		conn:     conn,
		nickname: nickname,
		ch:       make(chan Message, 100),
		lastSend: time.Now(),
	}

	slog.Debug("client created", "nickname", nickname, "remote", conn.RemoteAddr().String())
	go client.writeLoop()
	return client
}

func (c *Client) Listen() {
	slog.Debug("listen started", "nickname", c.nickname)

	defer func() {
		if r := recover(); r != nil {
			slog.Error("client panic", "nickname", c.nickname, "panic", r)
		}
		slog.Debug("listen stopped", "nickname", c.nickname)
	}()

	c.conn.SetReadDeadline(time.Time{})

	reader := bufio.NewReader(c.conn)
	messageCount := 0

	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			slog.Debug("client read stopped", "nickname", c.nickname, "error", err)
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			slog.Debug("empty line", "nickname", c.nickname)
			continue
		}

		messageCount++
		if messageCount > MaxMessagesPerLoop {
			slog.Debug("message limit pause", "nickname", c.nickname)
			time.Sleep(time.Duration(MinSendIntervalMs) * time.Millisecond)
			messageCount = 0
		}

		if !isSecureJsonString(line) {
			slog.Warn("invalid message", "nickname", c.nickname)
			continue
		}

		var incoming Message
		if err := json.Unmarshal([]byte(line), &incoming); err != nil {
			slog.Warn("invalid message", "nickname", c.nickname, "error", err)
			continue
		}

		now := time.Now()
		if now.Sub(c.lastSend) < time.Duration(MinSendIntervalMs)*time.Millisecond {
			slog.Debug("rate limited", "nickname", c.nickname)
			continue
		}
		c.lastSend = now

		sanitizedText := sanitizeString(incoming.Text)
		if !isValidMessage(sanitizedText) {
			slog.Warn("invalid message content", "nickname", c.nickname)
			continue
		}

		msg := Message{From: c.nickname, Text: sanitizedText}
		slog.Debug("message accepted", "nickname", c.nickname, "bytes", len(sanitizedText))

		select {
		case broadcast <- msg:
		default:
			slog.Warn("broadcast queue full", "nickname", c.nickname)
		}
	}
}

func (c *Client) Send(msg Message) {
	select {
	case c.ch <- msg:
		slog.Debug("message queued", "nickname", c.nickname)
	default:
		slog.Warn("message dropped", "nickname", c.nickname)
	}
}

func (c *Client) writeLoop() {
	slog.Debug("write loop started", "nickname", c.nickname)

	defer func() {
		if r := recover(); r != nil {
			slog.Error("write loop panic", "nickname", c.nickname, "panic", r)
		}
		slog.Debug("write loop stopped", "nickname", c.nickname)
	}()

	encoder := json.NewEncoder(c.conn)

	for msg := range c.ch {
		c.conn.SetWriteDeadline(time.Now().Add(time.Duration(WriteTimeoutMs) * time.Millisecond))

		if err := encoder.Encode(msg); err != nil {
			slog.Error("write failed", "nickname", c.nickname, "error", err)
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

func broadcastLoop() {
	for msg := range broadcast {
		mutex.RLock()
		for _, c := range clients {
			if c.nickname == msg.From {
				continue
			}
			select {
			case c.ch <- msg:
			default:
				slog.Warn("message dropped", "nickname", c.nickname)
			}
		}
		mutex.RUnlock()
	}
}
