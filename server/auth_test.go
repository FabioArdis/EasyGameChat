package server

import (
	"strings"
	"testing"
	"time"

	"github.com/golang-jwt/jwt/v4"
)

func testSecret() []byte {
	return []byte(strings.Repeat("s", MinJWTSecretLength))
}

func TestGenerateTokenAndValidate(t *testing.T) {
	secret := testSecret()

	token, err := GenerateToken("alice", secret, time.Hour)
	if err != nil {
		t.Fatal(err)
	}

	username, err := validateToken(token, secret)
	if err != nil {
		t.Fatal(err)
	}
	if username != "alice" {
		t.Errorf("username = %q, want alice", username)
	}
}

func TestGenerateTokenRejectsInvalidInput(t *testing.T) {
	secret := testSecret()

	tests := []struct {
		name     string
		username string
		secret   []byte
	}{
		{name: "short secret", username: "alice", secret: []byte("short")},
		{name: "empty username", username: "", secret: secret},
		{name: "reserved username", username: "admin", secret: secret},
		{name: "invalid username", username: "alice!", secret: secret},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if _, err := GenerateToken(tt.username, tt.secret, time.Hour); err == nil {
				t.Fatal("expected an error")
			}
		})
	}
}

func TestValidateTokenRejectsExpired(t *testing.T) {
	secret := testSecret()

	token, err := GenerateToken("alice", secret, -time.Hour)
	if err != nil {
		t.Fatal(err)
	}

	if _, err := validateToken(token, secret); err == nil {
		t.Fatal("expected an error")
	}
}

func TestValidateTokenRejectsWrongSecret(t *testing.T) {
	secret := testSecret()
	token, err := GenerateToken("alice", secret, time.Hour)
	if err != nil {
		t.Fatal(err)
	}

	otherSecret := []byte(strings.Repeat("t", MinJWTSecretLength))
	if _, err := validateToken(token, otherSecret); err == nil {
		t.Fatal("expected an error")
	}
}

func TestValidateTokenRejectsMissingExpiration(t *testing.T) {
	secret := testSecret()
	claims := tokenClaims{Username: "alice"}

	token, err := jwt.NewWithClaims(jwt.SigningMethodHS256, claims).SignedString(secret)
	if err != nil {
		t.Fatal(err)
	}

	if _, err := validateToken(token, secret); err == nil {
		t.Fatal("expected an error")
	}
}

func TestValidateTokenRejectsMissingUsername(t *testing.T) {
	secret := testSecret()
	claims := tokenClaims{
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Hour)),
		},
	}

	token, err := jwt.NewWithClaims(jwt.SigningMethodHS256, claims).SignedString(secret)
	if err != nil {
		t.Fatal(err)
	}

	if _, err := validateToken(token, secret); err == nil {
		t.Fatal("expected an error")
	}
}

func TestValidateTokenRejectsWrongAlgorithm(t *testing.T) {
	secret := testSecret()
	claims := tokenClaims{
		Username: "alice",
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Hour)),
		},
	}

	token, err := jwt.NewWithClaims(jwt.SigningMethodHS512, claims).SignedString(secret)
	if err != nil {
		t.Fatal(err)
	}

	if _, err := validateToken(token, secret); err == nil {
		t.Fatal("expected an error")
	}
}

func TestValidateTokenRejectsMalformedToken(t *testing.T) {
	if _, err := validateToken("not-a-token", testSecret()); err == nil {
		t.Fatal("expected an error")
	}
}
