package server

import (
	"strings"
	"unicode"
)

const (
	MaxNicknameLength = 32
	MaxMessageLength  = 512
	MaxBufferSize     = 4096
)

// Validation should happen on server too

func isValidNickname(nickname string) bool {
	if len(nickname) == 0 || len(nickname) > MaxNicknameLength {
		return false
	}

	// Must start with alphanumeric
	if !isAlphaNumeric(rune(nickname[0])) {
		return false
	}

	// Only allow alphanumeric, underscore, hyphen (no consecutive special chars)
	lastWasSpecial := false
	for _, r := range nickname {
		if !isAlphaNumeric(r) && r != '_' && r != '-' {
			return false
		}
		currentIsSpecial := (r == '_' || r == '-')
		if currentIsSpecial && lastWasSpecial {
			return false // No consecutive special characters
		}
		lastWasSpecial = currentIsSpecial
	}

	// Reserved names check
	lower := strings.ToLower(nickname)
	reserved := []string{"server", "admin", "system", "null", "undefined"}
	for _, reserved := range reserved {
		if lower == reserved {
			return false
		}
	}

	return true
}

func isValidMessage(message string) bool {
	if len(message) == 0 || len(message) > MaxMessageLength {
		return false
	}

	// Strict character validation - only printable ASCII + space
	for _, r := range message {
		if r < 32 || r > 126 {
			if r != ' ' { // Allow spaces
				return false
			}
		}
	}

	// No message can be only whitespace
	if strings.TrimSpace(message) == "" {
		return false
	}

	return true
}

func isSecureJsonString(str string) bool {
	if len(str) > MaxBufferSize {
		return false
	}

	// Must start and end with braces
	if len(str) == 0 || str[0] != '{' || str[len(str)-1] != '}' {
		return false
	}

	// Count braces to prevent malformed JSON
	braceCount := 0
	inString := false
	escaped := false

	for _, r := range str {
		if escaped {
			escaped = false
			continue
		}

		if r == '\\' && inString {
			escaped = true
			continue
		}

		if r == '"' {
			inString = !inString
			continue
		}

		if !inString {
			if r == '{' {
				braceCount++
			} else if r == '}' {
				braceCount--
			}
		}
	}

	return braceCount == 0 && !inString
}

func isAlphaNumeric(r rune) bool {
	return unicode.IsLetter(r) || unicode.IsDigit(r)
}

func sanitizeString(s string) string {
	// Remove any control characters except space
	var result strings.Builder
	for _, r := range s {
		if r >= 32 && r <= 126 {
			result.WriteRune(r)
		} else if r == ' ' {
			result.WriteRune(r)
		}
		// Skip all other characters
	}
	return strings.TrimSpace(result.String())
}
