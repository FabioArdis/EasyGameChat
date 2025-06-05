package server

import (
	"encoding/json"
	"fmt"
	"log"
)

type Message struct {
	From string `json:"from"`
	Text string `json:"text"`
}

// Validate ensures the message contains only expected fields and valid content
func (m *Message) Validate() error {
	log.Printf("[VALIDATION] Validating message from: %s", m.From)

	if !isValidNickname(m.From) {
		log.Printf("[VALIDATION] Invalid sender nickname: %s", m.From)
		return fmt.Errorf("invalid sender nickname: %s", m.From)
	}

	if !isValidMessage(m.Text) {
		log.Printf("[VALIDATION] Invalid message text from %s", m.From)
		return fmt.Errorf("invalid message text")
	}

	log.Printf("[VALIDATION] Message validation successful for %s", m.From)
	return nil
}

// SecureUnmarshal safely unmarshals JSON with validation
func SecureUnmarshal(data []byte) (*Message, error) {
	log.Printf("[UNMARSHAL] Attempting to unmarshal %d bytes of data", len(data))
	log.Printf("[UNMARSHAL] Raw data: %s", string(data))

	// First check if JSON structure is safe
	if !isSecureJsonString(string(data)) {
		log.Printf("[UNMARSHAL] Unsafe JSON structure detected")
		return nil, fmt.Errorf("unsafe JSON structure")
	}

	var msg Message
	if err := json.Unmarshal(data, &msg); err != nil {
		log.Printf("[UNMARSHAL] JSON unmarshal error: %v", err)
		return nil, fmt.Errorf("JSON unmarshal error: %w", err)
	}

	log.Printf("[UNMARSHAL] Successfully unmarshaled message from: %s", msg.From)

	// Validate the content
	if err := msg.Validate(); err != nil {
		log.Printf("[UNMARSHAL] Message validation failed: %v", err)
		return nil, fmt.Errorf("message validation failed: %w", err)
	}

	log.Printf("[UNMARSHAL] Message successfully processed: [%s]: %s", msg.From, msg.Text)
	return &msg, nil
}

// String returns a safe string representation
func (m *Message) String() string {
	return fmt.Sprintf("[%s]: %s", m.From, m.Text)
}
