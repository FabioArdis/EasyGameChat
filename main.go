package main

import (
	"context"
	"flag"
	"fmt"
	"github.com/FabioArdis/EasyGameChat/server"
	"log/slog"
	"os"
	"os/signal"
	"strings"
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

func setupLogger(level, format string) error {
	var logLevel slog.Level
	switch strings.ToLower(level) {
	case "debug":
		logLevel = slog.LevelDebug
	case "info":
		logLevel = slog.LevelInfo
	case "warn":
		logLevel = slog.LevelWarn
	case "error":
		logLevel = slog.LevelError
	default:
		return fmt.Errorf("invalid log level %q", level)
	}

	options := &slog.HandlerOptions{Level: logLevel}
	switch strings.ToLower(format) {
	case "text":
		slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, options)))
	case "json":
		slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stderr, options)))
	default:
		return fmt.Errorf("invalid log format %q", format)
	}

	return nil
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
		dev       bool
		listen    string
		certFile  string
		keyFile   string
		logLevel  string
		logFormat string
	)
	flag.BoolVar(&dev, "dev", false, "run locally with authentication disabled")
	flag.StringVar(&listen, "listen", server.DefaultAddr, "listen address")
	flag.StringVar(&certFile, "cert", server.DefaultCertFile, "TLS certificate file")
	flag.StringVar(&keyFile, "key", server.DefaultKeyFile, "TLS key file")
	flag.StringVar(&logLevel, "log-level", "info", "log level: debug, info, warn, error")
	flag.StringVar(&logFormat, "log-format", "text", "log format: text or json")
	flag.Parse()

	if err := setupLogger(logLevel, logFormat); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}

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
			slog.Error("failed to prepare development certificate", "error", err)
			os.Exit(1)
		}
		cfg.DisableAuth = true
		slog.Warn("development mode: authentication disabled")
	} else {
		secret, err := loadJWTSecret()
		if err != nil {
			slog.Error("configuration error", "error", err)
			os.Exit(1)
		}
		cfg.JWTSecret = secret
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	if err := server.StartTLS(ctx, cfg); err != nil {
		slog.Error("server stopped", "error", err)
		os.Exit(1)
	}
	slog.Info("server stopped")
}
