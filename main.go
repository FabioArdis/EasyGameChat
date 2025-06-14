package main

import (
	"EasyGameChat/server"
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	if len(os.Args) > 1 && os.Args[1] == "generate-token" {
		if len(os.Args) != 3 {
			fmt.Println("Usage: main generate-token <username>")
			return
		}
		username := os.Args[2]
		token, err := server.GenerateToken(username)
		if err != nil {
			fmt.Println("Error generating token:", err)
			return
		}
		fmt.Println("Generated token:", token)
		fmt.Println("SAVE THIS TOKEN IF YOU WANT TO USE IT! The one saved in tokens.json is hashed using SHA-256.", token)
		return
	}

	fmt.Println("=== EasyGameChat Server ===")
	fmt.Println("Security-focused chat server")
	fmt.Println("Press Ctrl+C to stop")
	fmt.Println()

	// Handle graceful shutdown
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)

	go func() {
		<-c
		fmt.Println("\n[SERVER] Shutting down gracefully...")
		os.Exit(0)
	}()

	// Start the server in TLS mode
	server.StartTLS()
}
