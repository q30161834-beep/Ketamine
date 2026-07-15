// ─── Ketamine Standard Library: Networking ─────────────────────────────────────

/// TCP connection.
pub struct TcpStream { /* opaque */ }

impl TcpStream {
    pub fn connect(addr: str) -> TcpStream?

    pub fn read(self, buf: &mut [u8]) -> int?
    pub fn write(self, buf: [u8]) -> int?
    pub fn close(self)
    pub fn set_timeout(self, ms: int)
    pub fn peer_addr(self) -> str
    pub fn local_addr(self) -> str
}

/// TCP listener.
pub struct TcpListener { /* opaque */ }

impl TcpListener {
    pub fn bind(addr: str) -> TcpListener?
    pub fn accept(self) -> TcpStream?
    pub fn close(self)
    pub fn local_addr(self) -> str
}

/// UDP socket.
pub struct UdpSocket { /* opaque */ }

impl UdpSocket {
    pub fn bind(addr: str) -> UdpSocket?
    pub fn send_to(self, buf: [u8], addr: str) -> int?
    pub fn recv_from(self, buf: &mut [u8]) -> (int, str)?
    pub fn close(self)
    pub fn set_broadcast(self, enabled: bool)
    pub fn local_addr(self) -> str
}

/// DNS resolution.
pub fn resolve_host(host: str) -> [str]?
pub fn lookup_host(host: str) -> str?

// ─── HTTP ──────────────────────────────────────────────────────────────────────

pub mod http {
    pub enum Method { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS }

    pub struct Request {
        method:  Method,
        url:     str,
        headers: HashMap<str, str>,
        body:    [u8]
    }

    pub struct Response {
        status:  int,
        headers: HashMap<str, str>,
        body:    [u8]
    }

    impl Response {
        pub fn text(self) -> str
        pub fn json<T: Deserialize>(self) -> T?
        pub fn status_code(self) -> int
        pub fn is_ok(self) -> bool { return self.status >= 200 && self.status < 300 }
        pub fn header(self, name: str) -> str?
    }

    /// Perform an HTTP request.
    pub fn request(req: Request) -> Response?

    /// Quick GET request.
    pub fn get(url: str) -> Response?

    /// Quick POST request.
    pub fn post(url: str, body: [u8], content_type: str) -> Response?

    /// Quick JSON POST.
    pub fn post_json<T: Serialize>(url: str, body: T) -> Response?

    /// Start an HTTP server.
    pub struct Server { /* opaque */ }

    impl Server {
        pub fn listen(addr: str) -> Server?
        pub fn handle(self, path: str, handler: fn(Request) -> Response)
        pub fn handle_static(self, path: str, dir: str)
        pub fn close(self)
    }
}

// ─── WebSocket ────────────────────────────────────────────────────────────────

pub mod websocket {
    pub enum Message {
        Text(str),
        Binary([u8]),
        Ping([u8]),
        Pong([u8]),
        Close(int, str)
    }

    pub struct Connection { /* opaque */ }

    impl Connection {
        pub fn connect(url: str) -> Connection?
        pub fn send(self, msg: Message) -> bool
        pub fn recv(self) -> Message?
        pub fn close(self)
        pub fn is_open(self) -> bool
    }
}

// ─── TLS ──────────────────────────────────────────────────────────────────────

pub mod tls {
    pub fn connect(addr: str, hostname: str) -> TcpStream?
    pub fn wrap(stream: TcpStream, hostname: str) -> TcpStream?

    pub struct Config {
        cert_file: str,
        key_file:  str,
        ca_file:   str,
        min_version: str,
        insecure:  bool
    }
}

// ─── MQTT ─────────────────────────────────────────────────────────────────────

pub mod mqtt {
    pub struct Client { /* opaque */ }

    impl Client {
        pub fn connect(broker: str, client_id: str) -> Client?
        pub fn publish(self, topic: str, payload: [u8], qos: int)
        pub fn subscribe(self, topic: str, qos: int)
        pub fn on_message(self, cb: fn(topic: str, payload: [u8]))
        pub fn disconnect(self)
    }
}
