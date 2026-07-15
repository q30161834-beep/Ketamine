// WebSocket Chat Server in Ketamine — compiles to Go
// go get golang.org/x/net/websocket
// Run: ketc websocket_server.kt --target go -o ws.go && go run ws.go

import go::net::http
import go::net::websocket
import go::fmt
import go::sync
import go::log

struct Client {
    id:   str,
    conn: go::net::websocket::Conn
}

struct ChatRoom {
    clients: [Client],
    mutex:   go::sync::RWMutex
}

fn new_chat_room() -> ChatRoom {
    return ChatRoom {
        clients: [],
        mutex:   go::sync::RWMutex{}
    }
}

fn chat_room_add(room: ChatRoom, client: Client) {
    room.mutex.Lock()
    room.clients.append(client)
    room.mutex.Unlock()
    go::log::Println("Client connected: " + client.id)
}

fn chat_room_remove(room: ChatRoom, id: str) {
    room.mutex.Lock()
    mut let i = 0
    while i < room.clients.len() {
        if room.clients[i].id == id {
            room.clients.remove(i)
            go::log::Println("Client disconnected: " + id)
            return
        }
        i = i + 1
    }
    room.mutex.Unlock()
}

fn chat_room_broadcast(room: ChatRoom, msg: str, sender_id: str) {
    room.mutex.RLock()
    for client in room.clients {
        if client.id != sender_id {
            go::net::websocket::Message::Send(client.conn, msg)
        }
    }
    room.mutex.RUnlock()
}

fn handle_ws(room: ChatRoom, ws: go::net::websocket::Conn) {
    let id = ws.RemoteAddr().String()
    let client = Client { id: id, conn: ws }
    chat_room_add(room, client)

    loop {
        mut let msg = ""
        let err = go::net::websocket::Message::Receive(ws, msg)
        if err != null {
            break
        }
        go::log::Println("[" + id + "] " + msg)
        chat_room_broadcast(room, "[" + id + "] " + msg, id)
    }

    chat_room_remove(room, id)
    ws.Close()
}

fn main() -> int {
    let room = new_chat_room()

    go::net::http::Handle("/ws", go::net::websocket::Handler(fn(ws) {
        handle_ws(room, ws)
    }))

    go::net::http::HandleFunc("/", fn(w, r) {
        w.Write("WebSocket Chat — connect to /ws")
    })

    go::log::Println("Chat server on :8080")
    go::net::http::ListenAndServe(":8080", null)
    return 0
}
