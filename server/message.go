package server

import (
	"encoding/json"
	"fmt"
)

type Message struct {
	From string `json:"from"`
	Text string `json:"text"`
}

// Validate ensures the message contains only expected fields and valid content
func (m *Message) Validate() error {
	if !isValidNickname(m.From) {
		return fmt.Errorf("invalid sender nickname: %s", m.From)
	}

	if !isValidMessage(m.Text) {
		return fmt.Errorf("invalid message text")
	}

	return nil
}

// SecureUnmarshal safely unmarshals JSON with validation
func SecureUnmarshal(data []byte) (*Message, error) {
	// First check if JSON structure is safe
	if !isSecureJsonString(string(data)) {
		return nil, fmt.Errorf("unsafe JSON structure")
	}

	var msg Message
	if err := json.Unmarshal(data, &msg); err != nil {
		return nil, fmt.Errorf("JSON unmarshal error: %w", err)
	}

	// Validate the content
	if err := msg.Validate(); err != nil {
		return nil, fmt.Errorf("message validation failed: %w", err)
	}

	return &msg, nil
}

// String returns a safe string representation
func (m *Message) String() string {
	return fmt.Sprintf("[%s]: %s", m.From, m.Text)
}
