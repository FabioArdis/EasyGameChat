package server

import (
	"crypto/tls"
	"crypto/x509"
	"encoding/pem"
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

func TestEnsureDevCertificate(t *testing.T) {
	dir := t.TempDir()
	certFile := filepath.Join(dir, "certs", "dev.crt")
	keyFile := filepath.Join(dir, "certs", "dev.key")

	if err := EnsureDevCertificate(certFile, keyFile); err != nil {
		t.Fatal(err)
	}

	if _, err := tls.LoadX509KeyPair(certFile, keyFile); err != nil {
		t.Fatalf("load key pair: %v", err)
	}

	certPEM, err := os.ReadFile(certFile)
	if err != nil {
		t.Fatal(err)
	}
	block, _ := pem.Decode(certPEM)
	if block == nil {
		t.Fatal("decode certificate PEM")
	}
	certificate, err := x509.ParseCertificate(block.Bytes)
	if err != nil {
		t.Fatal(err)
	}

	for _, name := range []string{"localhost", "127.0.0.1", "::1"} {
		if err := certificate.VerifyHostname(name); err != nil {
			t.Errorf("verify hostname %q: %v", name, err)
		}
	}

	keyInfo, err := os.Stat(keyFile)
	if err != nil {
		t.Fatal(err)
	}
	if runtime.GOOS != "windows" && keyInfo.Mode().Perm() != 0600 {
		t.Errorf("key permissions = %o, want 600", keyInfo.Mode().Perm())
	}

	if err := EnsureDevCertificate(certFile, keyFile); err != nil {
		t.Fatalf("second call: %v", err)
	}

	after, err := os.ReadFile(certFile)
	if err != nil {
		t.Fatal(err)
	}
	if string(certPEM) != string(after) {
		t.Error("existing certificate was overwritten")
	}
}

func TestEnsureDevCertificateRejectsPartialPair(t *testing.T) {
	dir := t.TempDir()
	certFile := filepath.Join(dir, "dev.crt")
	keyFile := filepath.Join(dir, "dev.key")

	if err := os.WriteFile(certFile, []byte("certificate"), 0644); err != nil {
		t.Fatal(err)
	}

	if err := EnsureDevCertificate(certFile, keyFile); err == nil {
		t.Fatal("expected an error")
	}
}
