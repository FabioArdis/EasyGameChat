package main

import (
	"EasyGameChat/server"
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"
)

const (
	devAddr     = "127.0.0.1:3000"
	devCertFile = "certs/dev.crt"
	devKeyFile  = "certs/dev.key"

	jwtSecretEnv = "EASYGAMECHAT_JWT_SECRET"
)

func loadJWTSecret() ([]byte, error) {
	secret := []byte(os.Getenv(jwtSecretEnv))
	if len(secret) < server.MinJWTSecretLength {
		return nil, fmt.Errorf("%s must be at least %d characters", jwtSecretEnv, server.MinJWTSecretLength)
	}
	return secret, nil
}

func main() {
	if len(os.Args) > 1 && os.Args[1] == "generate-token" {
		if len(os.Args) != 3 {
			fmt.Fprintln(os.Stderr, "Usage: easygamechat generate-token <username>")
			os.Exit(1)
		}
		username := os.Args[2]
		secret, err := loadJWTSecret()
		if err != nil {
			fmt.Fprintln(os.Stderr, "Error:", err)
			os.Exit(1)
		}
		token, err := server.GenerateToken(username, secret, 24*time.Hour)
		if err != nil {
			fmt.Fprintln(os.Stderr, "Error generating token:", err)
			os.Exit(1)
		}
		fmt.Println("Generated token:", token)
		fmt.Println("Save this token now. It cannot be recovered.")
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
	} else {
		secret, err := loadJWTSecret()
		if err != nil {
			fmt.Fprintln(os.Stderr, "Error:", err)
			os.Exit(1)
		}
		cfg.JWTSecret = secret
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
