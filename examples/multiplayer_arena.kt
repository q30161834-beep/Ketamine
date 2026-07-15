// Multiplayer Arena — real-time game server in Ketamine (compiles to Go)
// Features: WebSocket, rooms, matchmaking, AI bots, leaderboard, stats
// Run: ketc multiplayer_arena.kt --target go -o arena.go && go run arena.go

import go::net::http
import go::net::websocket
import go::encoding::json
import go::sync
import go::log
import go::fmt
import go::time
import go::math::rand
import go::strconv
import go::sort
import go::os

// ─── Game Constants ────────────────────────────────────────────────────────────

const ARENA_WIDTH     = 800
const ARENA_HEIGHT    = 600
const PLAYER_RADIUS   = 20
const BULLET_RADIUS   = 5
const BULLET_SPEED    = 7
const MAX_PLAYERS     = 16
const MAX_SCORE       = 50
const BOT_COUNT       = 4
const RESPAWN_TIME    = 3.0
const GAME_TICK_MS    = 16  // ~60 FPS

// ─── Data Types ────────────────────────────────────────────────────────────────

struct Vec2 {
    x: float,
    y: float
}

struct Player {
    id:        str,
    name:      str,
    pos:       Vec2,
    vel:       Vec2,
    hp:        int,
    kills:     int,
    deaths:    int,
    score:     int,
    team:      int,
    is_bot:    bool,
    connected: bool,
    last_shot: float
}

struct Bullet {
    id:     str,
    pos:    Vec2,
    vel:    Vec2,
    owner:  str,
    alive:  bool,
    life:   int
}

struct GameState {
    players:   [Player],
    bullets:   [Bullet],
    status:    int,  // 0=waiting, 1=playing, 2=ended
    mode:      int,  // 0=FFA, 1=TDM
    time_left: float,
    map_id:    int
}

struct Room {
    id:       str,
    name:     str,
    state:    GameState,
    clients:  map<str, go::net::websocket::Conn>,
    mu:       go::sync::RWMutex,
    ticker:   *go::time::Ticker,
    created:  int64
}

struct PlayerProfile {
    id:       str,
    name:     str,
    wins:     int,
    losses:   int,
    kills:    int,
    deaths:   int,
    rating:   int,
    games:    int
}

// ─── Global State ──────────────────────────────────────────────────────────────

struct ServerState {
    rooms:    map<str, Room>,
    profiles: map<str, PlayerProfile>,
    mu:       go::sync::RWMutex,
    next_id:  int
}

// ─── Utilities ─────────────────────────────────────────────────────────────────

fn random_id() -> str {
    let b = go::crypto::rand::Read(8)
    return go::fmt::Sprintf("%x", b)
}

fn random_float() -> float {
    return go::rand::Float64()
}

fn random_range(min: float, max: float) -> float {
    return min + random_float() * (max - min)
}

fn clamp(v: float, lo: float, hi: float) -> float {
    if v < lo { return lo }
    if v > hi { return hi }
    return v
}

fn dist(a: Vec2, b: Vec2) -> float {
    let dx = a.x - b.x
    let dy = a.y - b.y
    return sqrt(dx * dx + dy * dy)
}

fn vec2_length(v: Vec2) -> float {
    return sqrt(v.x * v.x + v.y * v.y)
}

fn vec2_normalize(v: Vec2) -> Vec2 {
    let len = vec2_length(v)
    if len == 0.0 { return Vec2 { x: 0.0, y: 0.0 } }
    return Vec2 { x: v.x / len, y: v.y / len }
}

// ─── Player Profile ────────────────────────────────────────────────────────────

fn new_profile(id: str, name: str) -> PlayerProfile {
    return PlayerProfile {
        id:     id,
        name:   name,
        wins:   0,
        losses: 0,
        kills:  0,
        deaths: 0,
        rating: 1000,
        games:  0
    }
}

fn profile_record_kill(p: PlayerProfile, is_bot: bool) {
    p.kills = p.kills + 1
    if !is_bot {
        p.rating = p.rating + 15
    }
}

fn profile_record_death(p: PlayerProfile) {
    p.deaths = p.deaths + 1
    if p.rating > 100 {
        p.rating = p.rating - 10
    }
}

fn profile_record_game(p: PlayerProfile, won: bool) {
    p.games = p.games + 1
    if won {
        p.wins = p.wins + 1
        p.rating = p.rating + 25
    } else {
        p.losses = p.losses + 1
    }
}

// ─── Game State ────────────────────────────────────────────────────────────────

fn new_game_state(mode: int, map_id: int) -> GameState {
    return GameState {
        players:   [],
        bullets:   [],
        status:    0,
        mode:      mode,
        time_left: 180.0,
        map_id:    map_id
    }
}

fn game_add_player(gs: GameState, p: Player) {
    gs.players.append(p)
}

fn game_remove_player(gs: GameState, id: str) {
    mut let i = 0
    while i < gs.players.len() {
        if gs.players[i].id == id {
            gs.players.remove(i)
            return
        }
        i = i + 1
    }
}

fn game_find_player(gs: GameState, id: str) -> Player {
    for p in gs.players {
        if p.id == id {
            return p
        }
    }
    return null
}

fn game_get_scores(gs: GameState) -> map<str, int> {
    let scores: map<str, int> = {}
    for p in gs.players {
        scores[p.name] = p.kills
    }
    return scores
}

fn game_get_winner(gs: GameState) -> str {
    if gs.players.len() == 0 { return "" }
    mut let best = gs.players[0]
    for p in gs.players {
        if p.kills > best.kills {
            best = p
        }
    }
    return best.name
}

fn game_get_team_score(gs: GameState, team: int) -> int {
    mut let total = 0
    for p in gs.players {
        if p.team == team {
            total = total + p.kills
        }
    }
    return total
}

// ─── Bot AI ────────────────────────────────────────────────────────────────────

fn bot_spawn(gs: GameState, id: str, name: str, team: int) -> Player {
    let pos = Vec2 {
        x: random_range(50.0, ARENA_WIDTH  - 50.0),
        y: random_range(50.0, ARENA_HEIGHT - 50.0)
    }
    return Player {
        id:        id,
        name:      name,
        pos:       pos,
        vel:       Vec2 { x: 0.0, y: 0.0 },
        hp:        100,
        kills:     0,
        deaths:    0,
        score:     0,
        team:      team,
        is_bot:    true,
        connected: true,
        last_shot: 0.0
    }
}

fn bot_spawn_all(gs: GameState) {
    mut let i = 0
    while i < BOT_COUNT {
        let bot_id = "bot_" + str(i)
        let bot_name = "Bot-" + str(i + 1)
        let team = i % 2
        let bot = bot_spawn(gs, bot_id, bot_name, team)
        game_add_player(gs, bot)
        i = i + 1
    }
}

fn bot_find_target(gs: GameState, bot: Player) -> Player {
    mut let closest: Player = null
    mut let closest_dist = 999999.0

    for p in gs.players {
        if p.id == bot.id { continue }
        if p.hp <= 0 { continue }
        if gs.mode == 1 && p.team == bot.team { continue }

        let d = dist(bot.pos, p.pos)
        if d < closest_dist {
            closest_dist = d
            closest = p
        }
    }
    return closest
}

fn bot_find_powerup(gs: GameState, bot: Player) -> Vec2 {
    return Vec2 { x: ARENA_WIDTH / 2.0, y: ARENA_HEIGHT / 2.0 }
}

fn bot_tick(gs: GameState, mut bot: Player, current_time: float) {
    if bot.hp <= 0 { return }

    let target = bot_find_target(gs, bot)
    if target == null { return }

    let d = dist(bot.pos, target.pos)

    // Move toward target
    let dir = vec2_normalize(Vec2 {
        x: target.pos.x - bot.pos.x,
        y: target.pos.y - bot.pos.y
    })

    let speed = 3.0
    bot.pos.x = bot.pos.x + dir.x * speed
    bot.pos.y = bot.pos.y + dir.y * speed

    // Clamp to arena
    bot.pos.x = clamp(bot.pos.x, PLAYER_RADIUS, ARENA_WIDTH  - PLAYER_RADIUS)
    bot.pos.y = clamp(bot.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS)

    // Shoot if close enough
    if d < 300.0 && current_time - bot.last_shot > 0.5 {
        bot.last_shot = current_time
        let bullet = Bullet {
            id:    random_id(),
            pos:   Vec2 { x: bot.pos.x, y: bot.pos.y },
            vel:   Vec2 { x: dir.x * BULLET_SPEED, y: dir.y * BULLET_SPEED },
            owner: bot.id,
            alive: true,
            life:  120
        }
        gs.bullets.append(bullet)
    }
}

// ─── Collision & Physics ───────────────────────────────────────────────────────

fn check_collisions(gs: GameState) {
    mut let i = 0
    while i < gs.bullets.len() {
        let b = gs.bullets[i]
        if !b.alive {
            i = i + 1
            continue
        }

        b.life = b.life - 1
        if b.life <= 0 {
            b.alive = false
            i = i + 1
            continue
        }

        // Move bullet
        b.pos.x = b.pos.x + b.vel.x
        b.pos.y = b.pos.y + b.vel.y

        // Check bounds
        if b.pos.x < 0 || b.pos.x > ARENA_WIDTH ||
           b.pos.y < 0 || b.pos.y > ARENA_HEIGHT {
            b.alive = false
            i = i + 1
            continue
        }

        // Check collision with players
        mut let j = 0
        while j < gs.players.len() {
            let p = gs.players[j]
            if p.id == b.owner { j = j + 1; continue }
            if p.hp <= 0 { j = j + 1; continue }

            let d = dist(b.pos, p.pos)
            if d < PLAYER_RADIUS + BULLET_RADIUS {
                p.hp = p.hp - 25
                b.alive = false

                if p.hp <= 0 {
                    // Find shooter
                    let shooter = game_find_player(gs, b.owner)
                    if shooter != null {
                        shooter.kills = shooter.kills + 1
                    }
                    p.deaths = p.deaths + 1
                }
                break
            }
            j = j + 1
        }
        i = i + 1
    }

    // Respawn dead players
    for p in gs.players {
        if p.hp <= 0 {
            p.hp = 100
            p.pos.x = random_range(50.0, ARENA_WIDTH  - 50.0)
            p.pos.y = random_range(50.0, ARENA_HEIGHT - 50.0)
        }
    }
}

// ─── Game Loop ─────────────────────────────────────────────────────────────────

fn game_tick(room: Room) {
    room.mu.Lock()

    let gs = room.state
    if gs.status != 1 {
        room.mu.Unlock()
        return
    }

    // Bots
    let t = go::time::Now().Unix()
    for p in gs.players {
        if p.is_bot {
            bot_tick(gs, p, t)
        }
    }

    // Physics
    check_collisions(gs)

    // Check win condition (simplified)
    for p in gs.players {
        if p.kills >= MAX_SCORE {
            gs.status = 2
            break
        }
    }

    // Broadcast state
    let state_json = go::encoding::json::Marshal(gs)
    for client in room.clients {
        go::net::websocket::Message::Send(client, state_json)
    }

    room.mu.Unlock()
}

fn start_game_loop(room: Room) {
    room.ticker = go::time::NewTicker(GAME_TICK_MS * go::time::Millisecond)
    loop {
        <-room.ticker.C
        game_tick(room)
    }
}

// ─── Room Management ───────────────────────────────────────────────────────────

fn create_room(ss: ServerState, name: str, mode: int, map_id: int) -> Room {
    let id = random_id()
    let room = Room {
        id:      id,
        name:    name,
        state:   new_game_state(mode, map_id),
        clients: {},
        mu:      go::sync::RWMutex{},
        ticker:  null,
        created: go::time::Now().Unix()
    }

    ss.mu.Lock()
    ss.rooms[id] = room
    ss.mu.Unlock()

    go::log::Println("Room created: " + name + " (id=" + id + ")")
    return room
}

fn find_or_create_room(ss: ServerState, mode: int) -> Room {
    ss.mu.RLock()
    for room in ss.rooms {
        let gs = room.state
        if gs.status == 0 && gs.mode == mode && gs.players.len() < MAX_PLAYERS {
            ss.mu.RUnlock()
            return room
        }
    }
    ss.mu.RUnlock()

    let names = ["Deathmatch Arena", "Cyber Stadium", "Neon District", "Crimson Citadel", "Void Chamber"]
    let name = names[go::rand::Intn(5)]
    return create_room(ss, name, mode, go::rand::Intn(3))
}

fn player_join_room(ss: ServerState, room: Room, conn: go::net::websocket::Conn, name: str) {
    let player_id = random_id()

    // Get or create profile
    ss.mu.Lock()
    let profile = ss.profiles[player_id]
    if profile == null {
        profile = new_profile(player_id, name)
        ss.profiles[player_id] = profile
    }
    ss.mu.Unlock()

    room.mu.Lock()
    room.clients[player_id] = conn

    let player = Player {
        id:        player_id,
        name:      name,
        pos:       Vec2 { x: random_range(50.0, 200.0), y: random_range(50.0, 200.0) },
        vel:       Vec2 { x: 0.0, y: 0.0 },
        hp:        100,
        kills:     0,
        deaths:    0,
        score:     0,
        team:      room.state.players.len() % 2,
        is_bot:    false,
        connected: true,
        last_shot: 0.0
    }
    game_add_player(room.state, player)
    room.mu.Unlock()

    go::log::Println("Player " + name + " joined room " + room.name)
}

fn player_leave_room(room: Room, player_id: str) {
    room.mu.Lock()
    game_remove_player(room.state, player_id)
    delete(room.clients, player_id)
    room.mu.Unlock()

    go::log::Println("Player " + player_id + " left")
}

// ─── WebSocket Handler ─────────────────────────────────────────────────────────

fn handle_connection(ss: ServerState, ws: go::net::websocket::Conn) {
    let player_name = "Player-" + str(go::rand::Intn(9999))
    let room = find_or_create_room(ss, 0)

    player_join_room(ss, room, ws, player_name)

    if room.state.players.len() >= 2 && room.state.status == 0 {
        room.mu.Lock()
        room.state.status = 1
        bot_spawn_all(room.state)
        room.mu.Unlock()

        let initial_state = go::encoding::json::Marshal(room.state)
        for client in room.clients {
            go::net::websocket::Message::Send(client, initial_state)
        }
    }

    // Read messages
    loop {
        mut let msg = ""
        let err = go::net::websocket::Message::Receive(ws, msg)
        if err != null {
            break
        }

        // Parse player input
        let input: map<str, int> = go::encoding::json::Unmarshal(msg)
        room.mu.Lock()
        let player = game_find_player(room.state, player_name)
        if player != null {
            // Movement
            let speed = 5.0
            if input["up"] == 1    { player.pos.y = player.pos.y - speed }
            if input["down"] == 1  { player.pos.y = player.pos.y + speed }
            if input["left"] == 1  { player.pos.x = player.pos.x - speed }
            if input["right"] == 1 { player.pos.x = player.pos.x + speed }

            player.pos.x = clamp(player.pos.x, PLAYER_RADIUS, ARENA_WIDTH  - PLAYER_RADIUS)
            player.pos.y = clamp(player.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS)

            // Shooting
            if input["shoot"] == 1 {
                let t = go::time::Now().Unix()
                if t - player.last_shot > 0.25 {
                    player.last_shot = t
                    let dir = Vec2 {
                        x: input["aim_x"],
                        y: input["aim_y"]
                    }
                    let norm = vec2_normalize(dir)
                    let bullet = Bullet {
                        id:    random_id(),
                        pos:   Vec2 { x: player.pos.x, y: player.pos.y },
                        vel:   Vec2 { x: norm.x * BULLET_SPEED, y: norm.y * BULLET_SPEED },
                        owner: player.id,
                        alive: true,
                        life:  60
                    }
                    room.state.bullets.append(bullet)
                }
            }
        }
        room.mu.Unlock()
    }

    player_leave_room(room, player_name)
    ws.Close()
}

// ─── HTTP Handlers ─────────────────────────────────────────────────────────────

fn handle_index(w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    w.WriteHeader(200)
    w.Write("Multiplayer Arena Server — connect via WebSocket at /ws\n")
    w.Write("Endpoints:\n")
    w.Write("  GET  /          — this page\n")
    w.Write("  GET  /stats     — server statistics\n")
    w.Write("  GET  /rooms     — active rooms\n")
    w.Write("  GET  /leaderboard — top players\n")
    w.Write("  WS   /ws        — game connection\n")
}

fn handle_stats(ss: ServerState, w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    ss.mu.RLock()
    let stats = go::encoding::json::Marshal(map<str, int>{
        "rooms":    ss.rooms.len(),
        "players":  ss.profiles.len(),
        "active_games": 0
    })
    ss.mu.RUnlock()

    w.Header().Set("Content-Type", "application/json")
    w.Write(stats)
}

fn handle_leaderboard(ss: ServerState, w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    ss.mu.RLock()
    let profiles: [PlayerProfile] = []
    for p in ss.profiles {
        profiles.append(p)
    }
    ss.mu.RUnlock()

    // Sort by rating
    go::sort::Slice(profiles, fn(i: int, j: int) -> bool {
        return profiles[i].rating > profiles[j].rating
    })

    mut let html = "<html><body><h1>Leaderboard</h1><table border='1'>"
    html = html + "<tr><th>#</th><th>Name</th><th>Rating</th><th>W/L</th><th>K/D</th></tr>"

    mut let i = 0
    while i < profiles.len() && i < 50 {
        let p = profiles[i]
        let ratio = 0.0
        if p.deaths > 0 {
            ratio = p.kills / p.deaths
        }
        html = html + "<tr><td>" + str(i + 1) + "</td>"
        html = html + "<td>" + p.name + "</td>"
        html = html + "<td>" + str(p.rating) + "</td>"
        html = html + "<td>" + str(p.wins) + "/" + str(p.losses) + "</td>"
        html = html + "<td>" + go::fmt::Sprintf("%.2f", ratio) + "</td></tr>"
        i = i + 1
    }

    html = html + "</table></body></html>"
    w.Header().Set("Content-Type", "text/html")
    w.Write(html)
}

fn handle_rooms(ss: ServerState, w: go::net::http::ResponseWriter, r: go::net::http::Request) {
    ss.mu.RLock()

    mut let room_list: [map<str, str>] = []
    for room in ss.rooms {
        let info = map<str, str>{
            "id":     room.id,
            "name":   room.name,
            "mode":   str(room.state.mode),
            "status": str(room.state.status),
            "players": str(room.state.players.len())
        }
        room_list.append(info)
    }

    ss.mu.RUnlock()

    let json = go::encoding::json::Marshal(room_list)
    w.Header().Set("Content-Type", "application/json")
    w.Write(json)
}

// ─── Main ──────────────────────────────────────────────────────────────────────

fn main() -> int {
    let ss = ServerState {
        rooms:    {},
        profiles: {},
        mu:       go::sync::RWMutex{},
        next_id:  0
    }

    // Create initial game rooms
    go::log::Println("Starting Multiplayer Arena server...")

    let ffa_room = create_room(ss, "Free For All", 0, 0)
    let tdm_room = create_room(ss, "Team Deathmatch", 1, 1)

    go start_game_loop(ffa_room)
    go start_game_loop(tdm_room)

    // HTTP routes
    go::net::http::HandleFunc("/", handle_index)
    go::net::http::HandleFunc("/stats", fn(w, r) { handle_stats(ss, w, r) })
    go::net::http::HandleFunc("/leaderboard", fn(w, r) { handle_leaderboard(ss, w, r) })
    go::net::http::HandleFunc("/rooms", fn(w, r) { handle_rooms(ss, w, r) })

    go::net::http::Handle("/ws", go::net::websocket::Handler(fn(ws) {
        handle_connection(ss, ws)
    }))

    let port = go::os::Getenv("PORT")
    if port == "" { port = "8080" }

    go::log::Println("Server running on :" + port)
    go::log::Println("  Web UI:      http://localhost:" + port)
    go::log::Println("  Leaderboard: http://localhost:" + port + "/leaderboard")
    go::log::Println("  API Stats:   http://localhost:" + port + "/stats")
    go::log::Println("  Game rooms:  http://localhost:" + port + "/rooms")
    go::log::Println("  WebSocket:   ws://localhost:" + port + "/ws")

    go::net::http::ListenAndServe(":" + port, null)
    return 0
}
