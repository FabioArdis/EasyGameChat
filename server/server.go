package server

import (
	"fmt"
	"net"
	"sync"
	"time"
)

const (
	MaxClients        = 100
	ConnectionTimeout = 30 * time.Second
	ServerPort        = ":3000"
)

var (
	clients     = make(map[net.Conn]*Client)
	mutex       sync.RWMutex
	broadcast   = make(chan Message, 1000) // Buffered to prevent blocking
	clientCount int
)

func Start() {
	ln, err := net.Listen("tcp", ServerPort)
	if err != nil {
		panic(err)
	}
	defer ln.Close()

	fmt.Printf("[SERVER] Listening on port %s...\n", ServerPort)
	fmt.Printf("[SERVER] Max clients: %d\n", MaxClients)

	go broadcaster()

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

func handleConnection(conn net.Conn) {
	// Set connection timeout only for initial handshake
	conn.SetReadDeadline(time.Now().Add(ConnectionTimeout))

	client := NewClient(conn)
	if client == nil {
		return
	}

	RegisterClient(client)
	defer UnregisterClient(client)

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
