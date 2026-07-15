# Standard Library — I/O

## Terminal I/O

```ketamine
print(val);          // Print any value (int, str, bool, float)
print("hello");      // Print string
println(val);        // Print with newline
read_line() -> str;  // Read a line from stdin
```

## File I/O

```ketamine
let f = File::open("data.txt");     // -> Result<File>
let content = f.read_all();          // -> Result<str>
f.write("data");                     // -> Result
f.close();

let json = JSON::parse("{...}");     // -> Result<JSON>
let csv = CSV::read("data.csv");     // -> Result<CSV>
```

## Networking

```ketamine
// TCP
let client = TCP::connect("127.0.0.1:8080");
client.send("request");
let response = client.recv();

// UDP
let socket = UDP::bind("0.0.0.0:9000");
socket.send_to("data", "127.0.0.1:9001");

// HTTP
let resp = HTTP::get("https://example.com");
print(resp.status);
print(resp.body);

// WebSocket
let ws = WebSocket::connect("ws://server.com/game");
ws.send("move");
let msg = ws.recv();
```
