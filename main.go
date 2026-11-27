package main

import (
	"EasyGameChat/server"
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

const (
	devAddr     = "127.0.0.1:3000"
	devCertFile = "certs/dev.crt"
	devKeyFile  = "certs/dev.key"
)

func main() {
	if len(os.Args) > 1 && os.Args[1] == "generate-token" {
		if len(os.Args) != 3 {
			fmt.Println("Usage: easygamechat generate-token <username>")
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

	var (
		dev      bool
		listen   string
		certFile string
		keyFile  string
	)
	flag.BoolVar(&dev, "dev", false, "run locally with authentication disabled")
	flag.StringVar(&listen, "listen", server.DefaultAddr, "listen address")
	flag.StringVar(&certFile, "cert", server.DefaultCertFile, "TLS certificate file")
	flag.StringVar(&keyFile, "key", server.DefaultKeyFile, "TLS key file")
	flag.Parse()

	cfg := server.Config{
		Addr:     listen,
		CertFile: certFile,
		KeyFile:  keyFile,
	}

	if dev {
		setFlags := make(map[string]bool)
		flag.Visit(func(f *flag.Flag) {
			setFlags[f.Name] = true
		})

		if !setFlags["listen"] {
			cfg.Addr = devAddr
		}
		if !setFlags["cert"] {
			cfg.CertFile = devCertFile
		}
		if !setFlags["key"] {
			cfg.KeyFile = devKeyFile
		}
		if err := server.EnsureDevCertificate(cfg.CertFile, cfg.KeyFile); err != nil {
			fmt.Fprintln(os.Stderr, "Error preparing development certificate:", err)
			os.Exit(1)
		}
		cfg.DisableAuth = true
		fmt.Println("[WARN] Development mode: authentication disabled")
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	fmt.Println("=== EasyGameChat Server ===")
	fmt.Println("Security-focused chat server")
	fmt.Println("Press Ctrl+C to stop")
	fmt.Println()

	if err := server.StartTLS(ctx, cfg); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
	fmt.Println("[SERVER] Shutting down gracefully...")
}
