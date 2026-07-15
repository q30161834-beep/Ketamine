fn double(x: int) -> int { return x * 2; }
fn add_one(x: int) -> int { return x + 1; }

fn main() {
    let result = 5 |> double |> add_one;
}
