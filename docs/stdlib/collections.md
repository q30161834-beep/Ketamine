# Standard Library — Collections

## `Vec<T>`

Dynamic array (like Rust's `Vec` or C++'s `std::vector`).

```ketamine
let mut v = Vec::new();
v.push(1);
v.push(2);
let x = v.get(0);    // -> Option<&T>
let len = v.len();   // -> int
v.pop();             // -> Option<T>
v.clear();
```

### Methods

| Method | Description |
|--------|-------------|
| `new() -> Vec<T>` | Create empty vector |
| `push(val: T)` | Append element |
| `pop() -> Option<T>` | Remove and return last element |
| `get(idx: int) -> T` | Get element by index |
| `set(idx: int, val: T)` | Set element by index |
| `len() -> int` | Number of elements |
| `is_empty() -> bool` | Check if empty |
| `clear()` | Remove all elements |
| `sort()` | Sort in place |
| `reverse()` | Reverse in place |

## `HashMap<K, V>`

Hash map (like Rust's `HashMap` or Python's `dict`).

```ketamine
let mut m = HashMap::new();
m.insert("key", 42);
let v = m.get("key");    // -> Option<&V>
m.remove("key");
m.contains("key");       // -> bool
m.len();                 // -> int
```

## `LinkedList<T>`

Doubly-linked list.

```ketamine
let mut l = LinkedList::new();
l.push_front(1);
l.push_back(2);
let first = l.pop_front();
let last = l.pop_back();
```

## `BinaryHeap<T>`

Priority queue (max-heap).

```ketamine
let mut h = BinaryHeap::new();
h.push(5);
h.push(10);
let max = h.pop();    // -> Option<T>, returns 10 first
```
