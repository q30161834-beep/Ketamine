struct Point { x: int, y: int }

trait Display {
    fn to_string(self) -> str;
}

impl Display for Point {
    fn to_string(self) -> str {
        return "Point";
    }
}
