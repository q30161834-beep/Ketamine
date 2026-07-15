// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 10: Tic-Tac-Toe — un joc complet în terminal
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte aplicate:
//   - struct, enum, match
//   - funcții, parametri, return
//   - while, for, if/else
//   - mut, ref
//   - Vec (listă dinamică)
//
// ═══════════════════════════════════════════════════════════════════════════════

// Reprezintă o celulă pe tabla de joc
enum Cell {
    Empty,
    X,
    O,
}

// Reprezintă starea jocului
enum GameState {
    Playing,
    Won(Cell),     // cine a câștigat
    Draw,
}

// Tabla de joc 3x3
struct Board {
    cells: Vec<Cell>,
}

impl Board {
    fn new() -> Board {
        let mut cells = Vec::new();
        for i in 0..9 {
            cells.push(Cell::Empty);
        }
        return Board { cells: cells };
    }

    fn display(self) {
        for row in 0..3 {
            let mut line = "";
            for col in 0..3 {
                let idx = row * 3 + col;
                let sym = match self.cells.get(idx) {
                    Cell::Empty => { "." },
                    Cell::X => { "X" },
                    Cell::O => { "O" },
                };
                line = line + sym + " ";
            }
            print(line);
        }
    }

    fn make_move(self, pos: int, player: Cell) -> bool {
        if pos < 0 || pos >= 9 { return false; }
        let current = self.cells.get(pos);
        match current {
            Cell::Empty => {
                self.cells.set(pos, player);
                return true;
            },
            _ => { return false; },
        }
    }

    fn check_winner(self) -> Cell {
        // Combinații câștigătoare
        let lines = [
            (0, 1, 2), (3, 4, 5), (6, 7, 8),  // rânduri
            (0, 3, 6), (1, 4, 7), (2, 5, 8),  // coloane
            (0, 4, 8), (2, 4, 6),              // diagonale
        ];

        for i in 0..8 {
            let (a, b, c) = lines[i];
            let ca = self.cells.get(a);
            let cb = self.cells.get(b);
            let cc = self.cells.get(c);

            // Dacă toate trei sunt același jucător (nu Empty)
            match ca {
                Cell::Empty => { continue; },
                _ => {
                    if ca == cb && cb == cc {
                        return ca;
                    }
                },
            }
        }
        return Cell::Empty;
    }

    fn is_full(self) -> bool {
        for i in 0..9 {
            match self.cells.get(i) {
                Cell::Empty => { return false; },
                _ => {},
            }
        }
        return true;
    }
}

fn main() {
    let mut board = Board::new();
    let mut current_player = Cell::X;

    loop {
        // Afișează tabla
        board.display();

        // Verifică starea jocului
        let winner = board.check_winner();
        match winner {
            Cell::Empty => {
                // Jocul continuă
            },
            _ => {
                print("Jucătorul ");
                match winner {
                    Cell::X => { print("X"); },
                    Cell::O => { print("O"); },
                }
                print(" a câștigat!");
                break;
            },
        }

        if board.is_full() {
            print("Remiză!");
            break;
        }

        // Simulează o mutare (în realitate, citește de la tastatură)
        let pos = 4;  // centrul
        if board.make_move(pos, current_player) {
            // Schimbă jucătorul
            match current_player {
                Cell::X => { current_player = Cell::O; },
                _ => { current_player = Cell::X; },
            }
        }
    }
}
