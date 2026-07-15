// HTTP Server in Ketamine — compiles to Go
// Run: ketc http_server.kt --target go -o server.go && go run server.go

import go::net::http
import go::fmt
import go::log
import go::encoding::json

struct Response {
    status:  int,
    message: str,
    data:    str
}

fn handle_root(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    let resp = Response {
        status:  200,
        message: "OK",
        data:    "Welcome to Ketamine HTTP Server"
    }
    w.Header().Set("Content-Type", "application/json")
    go::encoding::json::NewEncoder(w).Encode(resp)
}

fn handle_health(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    w.WriteHeader(200)
    w.Write("healthy")
}

fn handle_echo(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    if r.Method != "POST" {
        w.WriteHeader(405)
        return
    }
    let body = r.Body
    go::encoding::json::NewEncoder(w).Encode(body)
}

fn main() -> int {
    let mux = go::net::http::NewServeMux()

    mux.HandleFunc("/",        handle_root)
    mux.HandleFunc("/health",  handle_health)
    mux.HandleFunc("/echo",    handle_echo)

    go::log::Println("Server listening on :8080")
    let err = go::net::http::ListenAndServe(":8080", mux)
    if err != null {
        go::log::Fatal(err)
    }
    return 0
}
