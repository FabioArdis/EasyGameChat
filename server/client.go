package server

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"strings"
)

type Client struct {
	conn     net.Conn
	nickname string
	ch       chan Message
}

func NewClient(conn net.Conn) *Client {
	decoder := json.NewDecoder(conn)

	var hello Message
	if err := decoder.Decode(&hello); err != nil {
		fmt.Println("[ERROR] Failed to read nickname:", err)
		conn.Close()
		return nil
	}

	client := &Client{
		conn:     conn,
		nickname: strings.TrimSpace(hello.Text),
		ch:       make(chan Message),
	}

	go client.writeLoop()
	return client
}

func (c *Client) Listen() {
	r := bufio.NewReader(c.conn)

	for {
		line, err := r.ReadString('\n')
		if err != nil {
			fmt.Printf("[DEBUG] ReadString error for %s: %v\n", c.nickname, err)
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		var incoming Message
		if err := json.Unmarshal([]byte(line), &incoming); err != nil {
			fmt.Println("[ERROR] Invalid message JSON from", c.nickname, ":", err)
			continue
		}

		msg := Message{From: c.nickname, Text: incoming.Text}
		broadcast <- msg
	}
}

func (c *Client) Send(msg Message) {
	c.ch <- msg
}

func (c *Client) writeLoop() {
	encoder := json.NewEncoder(c.conn)

	for msg := range c.ch {
		err := encoder.Encode(msg)
		if err != nil {
			fmt.Println("[ERROR] Could not send message to", c.nickname)
			break
		}
	}
}
