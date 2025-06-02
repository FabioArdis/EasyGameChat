package server

import (
	"fmt"
	"net"
	"sync"
)

var (
	clients   = make(map[net.Conn]*Client)
	mutex     sync.Mutex
	broadcast = make(chan Message)
)

func Start() {
	ln, err := net.Listen("tcp", ":3000")

	if err != nil {
		panic(err)
	}

	defer ln.Close()
	fmt.Println("[SERVER] Listening port 3000...")

	go broadcaster()

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println("[ERROR] Connection refused:", err)
			continue
		}
		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {
	client := NewClient(conn)
	if client == nil {
		return
	}

	RegisterClient(client)
	client.Listen()
	UnregisterClient(client)
}

func RegisterClient(c *Client) {
	mutex.Lock()
	clients[c.conn] = c
	mutex.Unlock()

	msg := Message{From: "Server", Text: c.nickname + " joined the chatroom"}
	broadcast <- msg
}

func UnregisterClient(c *Client) {
	mutex.Lock()
	delete(clients, c.conn)
	mutex.Unlock()

	msg := Message{From: "Server", Text: c.nickname + " left the chatroom"}
	broadcast <- msg
}

func broadcaster() {
	for msg := range broadcast {
		mutex.Lock()
		for _, c := range clients {
			if c.nickname != msg.From {
				c.Send(msg)
			}
		}
		mutex.Unlock()
	}
}
