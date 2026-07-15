// ─── Ketamine Standard Library: Cryptography ──────────────────────────────────

// ─── Hashing ──────────────────────────────────────────────────────────────────

pub fn sha256(data: [u8]) -> [u8; 32]
pub fn sha512(data: [u8]) -> [u8; 64]
pub fn sha3_256(data: [u8]) -> [u8; 32]
pub fn sha3_512(data: [u8]) -> [u8; 64]
pub fn blake2b(data: [u8], size: int) -> [u8]
pub fn blake2s(data: [u8], size: int) -> [u8]

pub fn md5(data: [u8]) -> [u8; 16]
pub fn crc32(data: [u8]) -> u32
pub fn crc64(data: [u8]) -> u64
pub fn fnv1a(data: [u8]) -> u64

// ─── HMAC ─────────────────────────────────────────────────────────────────────

pub fn hmac_sha256(key: [u8], data: [u8]) -> [u8; 32]
pub fn hmac_sha512(key: [u8], data: [u8]) -> [u8; 64]

// ─── Symmetric Encryption ─────────────────────────────────────────────────────

pub fn aes_encrypt(key: [u8; 32], iv: [u8; 16], data: [u8]) -> [u8]?
pub fn aes_decrypt(key: [u8; 32], iv: [u8; 16], data: [u8]) -> [u8]?
pub fn chacha20_encrypt(key: [u8; 32], nonce: [u8; 12], data: [u8]) -> [u8]
pub fn chacha20_decrypt(key: [u8; 32], nonce: [u8; 12], data: [u8]) -> [u8]
pub fn salsa20_encrypt(key: [u8; 32], nonce: [u8; 8], data: [u8]) -> [u8]

// ─── Asymmetric Encryption ────────────────────────────────────────────────────

pub fn rsa_generate_key(bits: int) -> (RsaPrivateKey, RsaPublicKey)?
pub fn rsa_encrypt(pub_key: RsaPublicKey, data: [u8]) -> [u8]?
pub fn rsa_decrypt(priv_key: RsaPrivateKey, data: [u8]) -> [u8]?
pub fn rsa_sign(priv_key: RsaPrivateKey, data: [u8]) -> [u8]?
pub fn rsa_verify(pub_key: RsaPublicKey, data: [u8], sig: [u8]) -> bool

pub struct RsaPrivateKey { /* opaque */ }
pub struct RsaPublicKey  { /* opaque */ }

pub fn ecdsa_generate_key(curve: str) -> (EcdsaPrivateKey, EcdsaPublicKey)?
pub fn ecdsa_sign(priv_key: EcdsaPrivateKey, data: [u8]) -> [u8]?
pub fn ecdsa_verify(pub_key: EcdsaPublicKey, data: [u8], sig: [u8]) -> bool

pub struct EcdsaPrivateKey { /* opaque */ }
pub struct EcdsaPublicKey  { /* opaque */ }

pub fn x25519_keygen() -> (X25519PrivateKey, X25519PublicKey)
pub fn x25519_shared_secret(priv: X25519PrivateKey, pub: X25519PublicKey) -> [u8; 32]

pub struct X25519PrivateKey { key: [u8; 32] }
pub struct X25519PublicKey  { key: [u8; 32] }

// ─── Key Derivation ───────────────────────────────────────────────────────────

pub fn pbkdf2_hmac_sha256(password: [u8], salt: [u8], iterations: int, dklen: int) -> [u8]
pub fn argon2i(password: [u8], salt: [u8], time: int, mem: int, threads: int, dklen: int) -> [u8]?
pub fn argon2id(password: [u8], salt: [u8], time: int, mem: int, threads: int, dklen: int) -> [u8]?
pub fn scrypt(password: [u8], salt: [u8], n: int, r: int, p: int, dklen: int) -> [u8]?

// ─── Random ───────────────────────────────────────────────────────────────────

pub fn random_bytes(len: int) -> [u8]
pub fn random_u32() -> u32
pub fn random_u64() -> u64
pub fn shuffle<T>(arr: &mut [T])
pub fn sample<T>(arr: [T], k: int) -> [T]?

// ─── Encoding ─────────────────────────────────────────────────────────────────

pub fn hex_encode(data: [u8]) -> str
pub fn hex_decode(s: str) -> [u8]?

pub fn base64_encode(data: [u8]) -> str
pub fn base64_decode(s: str) -> [u8]?
pub fn base64_url_encode(data: [u8]) -> str

pub fn base58_encode(data: [u8]) -> str
pub fn base58_decode(s: str) -> [u8]?

pub fn bech32_encode(hrp: str, data: [u8]) -> str?
pub fn bech32_decode(s: str) -> (str, [u8])?
