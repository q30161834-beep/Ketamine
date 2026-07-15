// TLS 1.3 Server in Ketamine — compiles to Go
// Generate certs: openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
// Run: ketc tls_server.kt --target go -o tls.go && go run tls.go

import go::net::http
import go::crypto::tls
import go::log
import go::fmt
import go::os

fn make_tls_config(cert: str, key: str) -> go::crypto::tls::Config {
    let pair = go::crypto::tls::LoadX509KeyPair(cert, key)

    return go::crypto::tls::Config {
        Certificates: [pair],
        MinVersion:   go::crypto::tls::VersionTLS13,
        CurvePreferences: [
            go::crypto::tls::CurveP256,
            go::crypto::tls::X25519
        ],
        CipherSuites: [
            go::crypto::tls::TLS_AES_256_GCM_SHA384,
            go::crypto::tls::TLS_CHACHA20_POLY1305_SHA256
        ],
        PreferServerCipherSuites: true
    }
}

fn handle_secure(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    let proto = r.TLS.Version
    go::fmt::Fprintf(w, "Secure connection established. Protocol: TLS 1.3\n")
    go::fmt::Fprintf(w, "Client: %s\n", r.RemoteAddr)
}

fn redirect_to_https(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    let target = "https://" + r.Host + r.URL.RequestURI()
    go::net::http::Redirect(w, r, target, 301)
}

fn main() -> int {
    let cert = go::os::Getenv("TLS_CERT")
    let key  = go::os::Getenv("TLS_KEY")

    if cert == "" { cert = "cert.pem" }
    if key  == "" { key  = "key.pem"  }

    let cfg = make_tls_config(cert, key)

    let mux = go::net::http::NewServeMux()
    mux.HandleFunc("/", handle_secure)

    // HTTP → HTTPS redirect on port 80
    go go::net::http::ListenAndServe(":8080", go::net::http::HandlerFunc(redirect_to_https))

    // TLS server on port 8443
    let server = go::net::http::Server {
        Addr:      ":8443",
        Handler:   mux,
        TLSConfig: cfg
    }

    go::log::Println("TLS server listening on :8443")
    let err = server.ListenAndServeTLS("", "")
    if err != null {
        go::log::Fatal(err)
    }

    return 0
}
