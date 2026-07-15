// P2P Mesh Network in Ketamine — compiles to Go
// Run: ketc p2p_mesh.kt --target go -o mesh.go && go run mesh.go

import go::net
import go::bufio
import go::fmt
import go::sync
import go::os
import go::log
import go::strings
import go::crypto::rand

struct Peer {
    id:        str,
    addr:      str,
    conn:      go::net::Conn,
    connected: bool
}

struct Mesh {
    peers:    [Peer],
    mu:       go::sync::RWMutex,
    self_id:  str,
    self_addr: str
}

fn rand_id() -> str {
    let b = go::crypto::rand::Read(4)
    return go::fmt::Sprintf("%x", b)
}

fn new_mesh(addr: str) -> Mesh {
    return Mesh {
        peers:     [],
        mu:        go::sync::RWMutex{},
        self_id:   rand_id(),
        self_addr: addr
    }
}

fn mesh_connect(m: Mesh, addr: str) -> bool {
    let conn = go::net::Dial("tcp", addr)
    if conn == null {
        go::log::Println("Failed to connect to " + addr)
        return false
    }

    let p = Peer {
        id:        addr,
        addr:      addr,
        conn:      conn,
        connected: true
    }

    m.mu.Lock()
    m.peers.append(p)
    m.mu.Unlock()

    go::log::Println("Connected to peer: " + addr)

    // Start reading from this peer in background
    go mesh_read_peer(m, p)
    return true
}

fn mesh_read_peer(m: Mesh, p: Peer) {
    let reader = go::bufio::NewReader(p.conn)
    loop {
        let line = reader.ReadString('\n')
        if line == "" {
            break
        }
        let msg = go::strings::TrimSpace(line)
        go::log::Println("[" + p.id + "] " + msg)
        mesh_broadcast(m, msg, p.id)
    }
    p.conn.Close()
    mesh_remove_peer(m, p.id)
}

fn mesh_broadcast(m: Mesh, msg: str, from_id: str) {
    m.mu.RLock()
    for peer in m.peers {
        if peer.id != from_id && peer.connected {
            peer.conn.Write((msg + "\n").to_bytes())
        }
    }
    m.mu.RUnlock()
}

fn mesh_remove_peer(m: Mesh, id: str) {
    m.mu.Lock()
    mut let i = 0
    while i < m.peers.len() {
        if m.peers[i].id == id {
            m.peers.remove(i)
            go::log::Println("Peer disconnected: " + id)
            return
        }
        i = i + 1
    }
    m.mu.Unlock()
}

fn mesh_listen(m: Mesh) {
    let ln = go::net::Listen("tcp", m.self_addr)
    go::log::Println("Mesh node " + m.self_id + " listening on " + m.self_addr)

    loop {
        let conn = ln.Accept()
        let p = Peer {
            id:        conn.RemoteAddr().String(),
            addr:      conn.RemoteAddr().String(),
            conn:      conn,
            connected: true
        }
        m.mu.Lock()
        m.peers.append(p)
        m.mu.Unlock()
        go::log::Println("Peer joined: " + p.id)
        go mesh_read_peer(m, p)
    }
}

fn main() -> int {
    let port = go::os::Getenv("PORT")
    if port == "" {
        port = "9000"
    }

    let m = new_mesh(":" + port)

    // Connect to seed peer if provided
    let seed = go::os::Getenv("SEED")
    if seed != "" {
        mesh_connect(m, seed)
    }

    mesh_listen(m)
    return 0
}
