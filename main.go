package main

import (
	"EasyGameChat/server"
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

func main() {
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
