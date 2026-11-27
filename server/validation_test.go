package server

import (
	"strings"
	"testing"
)

func TestIsValidNickname(t *testing.T) {
	tests := []struct {
		name     string
		nickname string
		want     bool
	}{
		{name: "simple", nickname: "alice", want: true},
		{name: "letters and digits", nickname: "Alice-1", want: true},
		{name: "underscore", nickname: "a_b", want: true},
		{name: "maximum length", nickname: strings.Repeat("a", MaxNicknameLength), want: true},
		{name: "empty", nickname: "", want: false},
		{name: "too long", nickname: strings.Repeat("a", MaxNicknameLength+1), want: false},
		{name: "starts with hyphen", nickname: "-alice", want: false},
		{name: "invalid character", nickname: "alice!", want: false},
		{name: "consecutive underscores", nickname: "a__b", want: false},
		{name: "consecutive mixed specials", nickname: "a-_b", want: false},
		{name: "reserved name", nickname: "admin", want: false},
		{name: "reserved name different case", nickname: "SERVER", want: false},
		{name: "space", nickname: "al ice", want: false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := isValidNickname(tt.nickname); got != tt.want {
				t.Errorf("isValidNickname(%q) = %v, want %v", tt.nickname, got, tt.want)
			}
		})
	}
}

func TestIsValidMessage(t *testing.T) {
	tests := []struct {
		name    string
		message string
		want    bool
	}{
		{name: "plain text", message: "hello", want: true},
		{name: "spaces", message: "hello world", want: true},
		{name: "maximum length", message: strings.Repeat("a", MaxMessageLength), want: true},
		{name: "punctuation", message: "{}[]~", want: true},
		{name: "empty", message: "", want: false},
		{name: "whitespace only", message: "   ", want: false},
		{name: "too long", message: strings.Repeat("a", MaxMessageLength+1), want: false},
		{name: "newline", message: "hello\nworld", want: false},
		{name: "tab", message: "hello\tworld", want: false},
		{name: "non-ascii", message: "café", want: false},
		{name: "delete character", message: "hello\x7f", want: false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := isValidMessage(tt.message); got != tt.want {
				t.Errorf("isValidMessage(%q) = %v, want %v", tt.message, got, tt.want)
			}
		})
	}
}

func TestIsSecureJsonString(t *testing.T) {
	tests := []struct {
		name  string
		input string
		want  bool
	}{
		{name: "object", input: `{"from":"alice","text":"hello"}`, want: true},
		{name: "brace in string", input: `{"text":"}"}`, want: true},
		{name: "empty", input: "", want: false},
		{name: "not an object", input: "not json", want: false},
		{name: "unclosed object", input: `{"text":"hello"`, want: false},
		{name: "extra closing brace", input: `{"text":"hello"}}`, want: false},
		{name: "unterminated string", input: `{"text":"hello}`, want: false},
		{name: "oversized", input: "{" + strings.Repeat("a", MaxBufferSize) + "}", want: false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := isSecureJsonString(tt.input); got != tt.want {
				t.Errorf("isSecureJsonString(%q) = %v, want %v", tt.input, got, tt.want)
			}
		})
	}
}

func TestSanitizeString(t *testing.T) {
	tests := []struct {
		name  string
		input string
		want  string
	}{
		{name: "plain", input: "hello", want: "hello"},
		{name: "trims", input: "  hello  ", want: "hello"},
		{name: "removes control characters", input: "hello\nworld\t", want: "helloworld"},
		{name: "removes non-ascii", input: "café", want: "caf"},
		{name: "keeps printable characters", input: "hello world!", want: "hello world!"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := sanitizeString(tt.input); got != tt.want {
				t.Errorf("sanitizeString(%q) = %q, want %q", tt.input, got, tt.want)
			}
		})
	}
}
