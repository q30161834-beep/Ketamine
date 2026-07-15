// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 09: HTTP Server — un server web simplu
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - Server HTTP simplu
//   - Rutare pe baza de path
//   - Răspuns JSON
//   - Query parameters
//
// ═══════════════════════════════════════════════════════════════════════════════

import net::http;

struct Server {
    port: int,
    routes: Vec<Route>,
}

struct Route {
    path: str,
    handler: fn(Request) -> Response,
}

struct Request {
    method: str,
    path: str,
    body: str,
}

struct Response {
    status: int,
    body: str,
}

impl Server {
    fn new(port: int) -> Server {
        return Server {
            port: port,
            routes: Vec::new(),
        };
    }

    fn route(self, path: str, handler: fn(Request) -> Response) {
        self.routes.push(Route { path: path, handler: handler });
    }

    fn handle_request(self, req: Request) -> Response {
        for i in 0..self.routes.len() {
            let route = self.routes.get(i);
            if route.path == req.path {
                return route.handler(req);
            }
        }

        // 404 — nu s-a găsit ruta
        return Response {
            status: 404,
            body: "{ \"error\": \"not found\" }",
        };
    }

    fn start(self) {
        // În realitate, aici ar asculta pe un socket
        print("Server pornit pe portul ");
        print(self.port);
    }
}

fn hello_handler(req: Request) -> Response {
    return Response {
        status: 200,
        body: "{ \"message\": \"Salut, lume!\" }",
    };
}

fn main() {
    let mut server = Server::new(8080);
    server.route("/hello", hello_handler);

    let req = Request {
        method: "GET",
        path: "/hello",
        body: "",
    };

    let resp = server.handle_request(req);
    print(resp.status);
    print(resp.body);

    server.start();
}
