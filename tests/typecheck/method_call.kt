struct Point { x: int, y: int }

impl Point {
    fn get_x(self) -> int {
        return self.x;
    }
}

fn main() {
    let p = Point { x: 1, y: 2 };
    let v = p.get_x();
}
