#!/bin/bash

# Generate TLS certificates
# This creates self-signed certificates for development/testing

echo "Generating TLS certificates for EasyGameChat server..."

# Generate private key
openssl genrsa -out server.key 2048

# Generate certificate signing request
openssl req -new -key server.key -out server.csr \
    -subj "/C=US/ST=State/L=City/O=EasyGameChat/CN=localhost"

# Generate self-signed certificate (valid for 365 days)
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt \
    -extensions v3_req -extfile <(cat <<EOF
[v3_req]
keyUsage = keyEncipherment, dataEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = 127.0.0.1
IP.1 = 127.0.0.1
IP.2 = ::1
EOF
)

# Clean up CSR file
rm server.csr

echo "Certificates generated:"
echo "  - server.key (private key)"
echo "  - server.crt (certificate)"
echo ""
echo "For production, replace these with certificates from a trusted CA."
echo ""
echo "Certificate details:"
openssl x509 -in server.crt -text -noout | grep -A 1 "Subject:"