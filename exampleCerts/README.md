# Example Certificates

The certificate files in this directory are **not committed to source control**
because they are generated per-machine and tied to a specific Certificate Authority
(CA) and Public Key Infrastructure (PKI).

You must generate replacement certificates before running the example. The four
files the application expects are:

| File | Description |
|---|---|
| `iss-1.pem` | CA / issuer certificate (trusted root) |
| `opr-389000.pem` | Client (operational) certificate, signed by the CA |
| `key-389000.pem` | Private key for the client certificate (PKCS#8 PEM) |
| `csr-389000.pem` | Certificate signing request (intermediate artefact, not loaded at runtime) |

## Quick-start with OpenSSL

The commands below create a self-signed CA and a client certificate suitable
for testing against a local BACnet/SC Hub.

```bash
# 1. Generate the CA key and self-signed certificate
openssl genrsa -out ca-key.pem 2048
openssl req -new -x509 -days 3650 -key ca-key.pem -out iss-1.pem \
  -subj "/CN=BACnetSC-Test-CA"

# 2. Generate the client key (encrypted PKCS#8)
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 \
  -aes-256-cbc -pass pass:YOUR_PASSPHRASE \
  -out key-389000.pem

# 3. Create the CSR
openssl req -new -key key-389000.pem -passin pass:YOUR_PASSPHRASE \
  -out csr-389000.pem \
  -subj "/CN=BACnetSC-Test-Client"

# 4. Sign the client certificate with the CA
openssl x509 -req -days 3650 \
  -in csr-389000.pem -CA iss-1.pem -CAkey ca-key.pem -CAcreateserial \
  -out opr-389000.pem
```

Pass `YOUR_PASSPHRASE` to the application via the `--key-password` command-line
argument.

## Hub configuration

The Hub must be configured to trust `iss-1.pem` as its CA certificate and to
present a certificate signed by the same (or a mutually trusted) CA.
