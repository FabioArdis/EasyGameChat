@echo off
REM Generate TLS certificates for EasyGameChat server (Windows)

echo Generating TLS certificates for EasyGameChat server...

REM Set cert output directory
set CERTS_DIR=..\certs

REM Create certs directory if it doesn't exist
if not exist %CERTS_DIR% (
    mkdir %CERTS_DIR%
)

REM Create minimal openssl.cnf
set OPENSSL_CONF_TEMP=openssl.cnf
(
echo [req]
echo distinguished_name=req_distinguished_name
echo x509_extensions = v3_req
echo prompt=no
echo [req_distinguished_name]
echo CN=localhost
echo [v3_req]
echo keyUsage = keyEncipherment, dataEncipherment
echo extendedKeyUsage = serverAuth
echo subjectAltName = @alt_names
echo [alt_names]
echo DNS.1 = localhost
echo DNS.2 = 127.0.0.1
echo IP.1 = 127.0.0.1
echo IP.2 = ::1
) > %OPENSSL_CONF_TEMP%

set OPENSSL_CONF=%CD%\%OPENSSL_CONF_TEMP%

REM Generate private key
openssl genrsa -out %CERTS_DIR%\server.key 2048

REM Generate certificate signing request
openssl req -new -key %CERTS_DIR%\server.key -out %CERTS_DIR%\server.csr -config %OPENSSL_CONF%

REM Generate self-signed certificate
openssl x509 -req -days 365 -in %CERTS_DIR%\server.csr -signkey %CERTS_DIR%\server.key -out %CERTS_DIR%\server.crt -extensions v3_req -extfile %OPENSSL_CONF%

REM Clean up temporary files
del %OPENSSL_CONF_TEMP%
del %CERTS_DIR%\server.csr

echo.
echo Certificates generated in %CERTS_DIR%:
echo   - server.key (private key)
echo   - server.crt (certificate)
echo.
echo For production, replace these with certificates from a trusted CA.
echo.
echo Certificate details:
openssl x509 -in %CERTS_DIR%\server.crt -text -noout | findstr /C:"Subject:" /C:"DNS" /C:"IP Address"

pause
