// REST API with JSON in Ketamine — compiles to Go
// Run: ketc rest_api.kt --target go -o api.go && go run api.go

import go::net::http
import go::encoding::json
import go::log
import go::fmt
import go::sync
import go::strconv

// ── Models ────────────────────────────────────────────────────────────────────

struct User {
    id:       int,
    username: str,
    email:    str,
    active:   bool
}

struct UserStore {
    users:  [User],
    mu:     go::sync::RWMutex,
    next_id: int
}

// ── Store ─────────────────────────────────────────────────────────────────────

fn new_store() -> UserStore {
    return UserStore {
        users:   [],
        mu:      go::sync::RWMutex{},
        next_id: 1
    }
}

fn store_get_all(s: UserStore) -> [User] {
    s.mu.RLock()
    let result = s.users
    s.mu.RUnlock()
    return result
}

fn store_get(s: UserStore, id: int) -> User {
    s.mu.RLock()
    for u in s.users {
        if u.id == id {
            s.mu.RUnlock()
            return u
        }
    }
    s.mu.RUnlock()
    return null
}

fn store_create(s: UserStore, username: str, email: str) -> User {
    s.mu.Lock()
    let u = User {
        id:       s.next_id,
        username: username,
        email:    email,
        active:   true
    }
    s.users.append(u)
    s.next_id = s.next_id + 1
    s.mu.Unlock()
    return u
}

fn store_delete(s: UserStore, id: int) -> bool {
    s.mu.Lock()
    mut let i = 0
    while i < s.users.len() {
        if s.users[i].id == id {
            s.users.remove(i)
            s.mu.Unlock()
            return true
        }
        i = i + 1
    }
    s.mu.Unlock()
    return false
}

// ── Handlers ──────────────────────────────────────────────────────────────────

fn json_response(w: go::net::http::ResponseWriter, status: int, data: str) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    w.Write(data)
}

fn handle_users(store: UserStore, w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    if r.Method == "GET" {
        let users = store_get_all(store)
        let body  = go::encoding::json::Marshal(users)
        json_response(w, 200, body)
        return
    }

    if r.Method == "POST" {
        mut let input = User{}
        go::encoding::json::NewDecoder(r.Body).Decode(input)
        let created = store_create(store, input.username, input.email)
        let body    = go::encoding::json::Marshal(created)
        json_response(w, 201, body)
        return
    }

    w.WriteHeader(405)
}

fn handle_user(store: UserStore, w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    let id_str = r.URL.Path[len("/users/"):]
    let id     = go::strconv::Atoi(id_str)

    if r.Method == "GET" {
        let user = store_get(store, id)
        if user == null {
            json_response(w, 404, "{\"error\":\"not found\"}")
            return
        }
        let body = go::encoding::json::Marshal(user)
        json_response(w, 200, body)
        return
    }

    if r.Method == "DELETE" {
        let ok = store_delete(store, id)
        if ok {
            json_response(w, 200, "{\"deleted\":true}")
        } else {
            json_response(w, 404, "{\"error\":\"not found\"}")
        }
        return
    }

    w.WriteHeader(405)
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() -> int {
    let store = new_store()

    // Seed data
    store_create(store, "alice", "alice@example.com")
    store_create(store, "bob",   "bob@example.com")

    let mux = go::net::http::NewServeMux()
    mux.HandleFunc("/users",   fn(w, r) { handle_users(store, w, r) })
    mux.HandleFunc("/users/",  fn(w, r) { handle_user(store, w, r)  })
    mux.HandleFunc("/health",  fn(w, r) { json_response(w, 200, "{\"status\":\"ok\"}") })

    go::log::Println("REST API running on :8080")
    go::log::Println("  GET  /users")
    go::log::Println("  POST /users")
    go::log::Println("  GET  /users/:id")
    go::log::Println("  DELETE /users/:id")

    go::net::http::ListenAndServe(":8080", mux)
    return 0
}
