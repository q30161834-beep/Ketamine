// ─── Ketamine Standard Library: Collections ────────────────────────────────────
// Data structures and algorithms.

// ─── Dynamic Array (Vec) ──────────────────────────────────────────────────────

pub struct Vec<T> {
    data: *T,
    len:  int,
    cap:  int
}

impl<T> Vec<T> {
    pub fn new() -> Vec<T>
    pub fn with_capacity(cap: int) -> Vec<T>
    pub fn from_slice(slice: [T]) -> Vec<T>
    pub fn from_array(arr: [T; N]) -> Vec<T>

    pub fn push(self, val: T)
    pub fn pop(self) -> T?
    pub fn insert(self, index: int, val: T)
    pub fn remove(self, index: int) -> T?
    pub fn clear(self)

    pub fn get(self, index: int) -> T?
    pub fn get_mut(self, index: int) -> &mut T?
    pub fn set(self, index: int, val: T) -> bool

    pub fn first(self) -> T?
    pub fn last(self) -> T?
    pub fn first_mut(self) -> &mut T?
    pub fn last_mut(self) -> &mut T?

    pub fn len(self) -> int
    pub fn cap(self) -> int
    pub fn is_empty(self) -> bool
    pub fn reserve(self, additional: int)
    pub fn shrink_to_fit(self)
    pub fn resize(self, new_len: int, val: T)
    pub fn append(self, other: &mut Vec<T>)

    pub fn sort(self)
    pub fn sort_by(self, cmp: fn(a: &T, b: &T) -> int)
    pub fn reverse(self)
    pub fn contains(self, val: T) -> bool where T: Eq
    pub fn dedup(self) where T: Eq

    pub fn map<U>(self, f: fn(T) -> U) -> Vec<U>
    pub fn filter(self, f: fn(&T) -> bool) -> Vec<T>
    pub fn flat_map<U>(self, f: fn(T) -> Vec<U>) -> Vec<U>
    pub fn fold<U>(self, init: U, f: fn(U, T) -> U) -> U
    pub fn reduce(self, f: fn(T, T) -> T) -> T?
    pub fn for_each(self, f: fn(T) -> void)
    pub fn any(self, f: fn(&T) -> bool) -> bool
    pub fn all(self, f: fn(&T) -> bool) -> bool
    pub fn find(self, f: fn(&T) -> bool) -> T?
    pub fn position(self, f: fn(&T) -> bool) -> int?

    pub fn join(self, sep: str) -> str where T: Debug
    pub fn as_slice(self) -> [T]
    pub fn iter(self) -> VecIter<T>
}

impl<T: Clone> Vec<T> {
    pub fn clone(self) -> Vec<T>
    pub fn resize_default(self, new_len: int)
}

pub struct VecIter<T> {
    vec: *Vec<T>,
    pos: int
}

impl<T> Iterator for VecIter<T> {
    type Item = T
    fn next(self) -> Option<T>
    fn size_hint(self) -> (int, int?)
}

// ─── Linked List ──────────────────────────────────────────────────────────────

pub struct LinkedList<T> {
    head: *Node<T>,
    tail: *Node<T>,
    len:  int
}

struct Node<T> {
    val:  T,
    next: *Node<T>,
    prev: *Node<T>
}

impl<T> LinkedList<T> {
    pub fn new() -> LinkedList<T>
    pub fn push_front(self, val: T)
    pub fn push_back(self, val: T)
    pub fn pop_front(self) -> T?
    pub fn pop_back(self) -> T?
    pub fn front(self) -> T?
    pub fn back(self) -> T?
    pub fn len(self) -> int
    pub fn is_empty(self) -> bool
    pub fn clear(self)
    pub fn iter(self) -> LinkedListIter<T>
}

pub struct LinkedListIter<T> {
    node: *Node<T>
}

// ─── HashMap ──────────────────────────────────────────────────────────────────

pub struct HashMap<K, V> {
    buckets: *Bucket<K, V>,
    len:     int,
    cap:     int,
    load:    float
}

struct Bucket<K, V> {
    key:   K,
    val:   V,
    next:  *Bucket<K, V>,
    filled: bool
}

impl<K: Hash + Eq, V> HashMap<K, V> {
    pub fn new() -> HashMap<K, V>
    pub fn with_capacity(cap: int) -> HashMap<K, V>

    pub fn insert(self, key: K, val: V)
    pub fn get(self, key: K) -> V?
    pub fn get_mut(self, key: K) -> &mut V?
    pub fn remove(self, key: K) -> V?
    pub fn contains_key(self, key: K) -> bool
    pub fn len(self) -> int
    pub fn is_empty(self) -> bool
    pub fn clear(self)

    pub fn keys(self) -> Vec<K>
    pub fn values(self) -> Vec<V>
    pub fn iter(self) -> HashMapIter<K, V>
}

pub struct HashMapIter<K, V> {
    map: *HashMap<K, V>,
    pos: int
}

// ─── HashSet ───────────────────────────────────────────────────────────────────

pub struct HashSet<T: Hash + Eq> {
    map: HashMap<T, ()>
}

impl<T: Hash + Eq> HashSet<T> {
    pub fn new() -> HashSet<T>
    pub fn insert(self, val: T)
    pub fn contains(self, val: T) -> bool
    pub fn remove(self, val: T) -> bool
    pub fn len(self) -> int
    pub fn is_empty(self) -> bool
    pub fn clear(self)
    pub fn iter(self) -> HashSetIter<T>

    pub fn union(self, other: HashSet<T>) -> HashSet<T>
    pub fn intersection(self, other: HashSet<T>) -> HashSet<T>
    pub fn difference(self, other: HashSet<T>) -> HashSet<T>
    pub fn symmetric_diff(self, other: HashSet<T>) -> HashSet<T>
    pub fn is_subset(self, other: HashSet<T>) -> bool
    pub fn is_superset(self, other: HashSet<T>) -> bool
}

pub struct HashSetIter<T> {
    inner: HashMapIter<T, ()>
}

// ─── Binary Heap (Priority Queue) ─────────────────────────────────────────────

pub struct BinaryHeap<T: Ord> {
    data: Vec<T>
}

impl<T: Ord> BinaryHeap<T> {
    pub fn new() -> BinaryHeap<T>
    pub fn with_capacity(cap: int) -> BinaryHeap<T>
    pub fn from_vec(vec: Vec<T>) -> BinaryHeap<T>

    pub fn push(self, val: T)
    pub fn pop(self) -> T?
    pub fn peek(self) -> T?
    pub fn len(self) -> int
    pub fn is_empty(self) -> bool
    pub fn clear(self)
}

// ─── BTreeMap (ordered map) ───────────────────────────────────────────────────

pub struct BTreeMap<K: Ord, V> {
    root: *BNode<K, V>,
    len:  int
}

impl<K: Ord, V> BTreeMap<K, V> {
    pub fn new() -> BTreeMap<K, V>
    pub fn insert(self, key: K, val: V)
    pub fn get(self, key: K) -> V?
    pub fn remove(self, key: K) -> V?
    pub fn contains(self, key: K) -> bool
    pub fn len(self) -> int
    pub fn is_empty(self) -> bool
    pub fn clear(self)
    pub fn range(self, start: K, end: K) -> BTreeIter<K, V>
}

// ─── Sorting Algorithms ───────────────────────────────────────────────────────

pub fn sort<T: Ord>(arr: &mut [T])
pub fn sort_by<T>(arr: &mut [T], cmp: fn(a: &T, b: &T) -> int)
pub fn sort_stable<T: Ord>(arr: &mut [T])
pub fn is_sorted<T: Ord>(arr: [T]) -> bool
pub fn binary_search<T: Ord>(arr: [T], val: T) -> int?
pub fn partition<T>(arr: &mut [T], f: fn(&T) -> bool) -> int
pub fn merge<T: Ord>(a: [T], b: [T]) -> Vec<T>
pub fn unique<T: Ord>(arr: [T]) -> Vec<T>

// ─── String Algorithms ────────────────────────────────────────────────────────

pub fn levenshtein(a: str, b: str) -> int
pub fn hamming(a: str, b: str) -> int?
pub fn longest_common_subseq(a: str, b: str) -> str
pub fn longest_common_prefix(a: str, b: str) -> str
pub fn edit_distance(a: str, b: str) -> int
pub fn fuzzy_match(pattern: str, text: str) -> bool
pub fn glob_match(pattern: str, text: str) -> bool

// ─── Graph Algorithms ─────────────────────────────────────────────────────────

pub fn topological_sort<T>(graph: HashMap<T, Vec<T>>) -> Vec<T>?
pub fn bfs<T: Eq + Hash>(start: T, goal: T, graph: HashMap<T, Vec<T>>) -> Vec<T>?
pub fn dfs<T: Eq + Hash>(start: T, goal: T, graph: HashMap<T, Vec<T>>) -> Vec<T>?
pub fn dijkstra<T: Eq + Hash + Ord>(start: T, goal: T, graph: HashMap<T, Vec<(T, int)>>) -> (Vec<T>, int)?
pub fn a_star<T: Eq + Hash + Ord>(start: T, goal: T, graph: HashMap<T, Vec<(T, int)>>, h: fn(T) -> int) -> (Vec<T>, int)?
pub fn floyd_warshall(graph: &mut [Vec<int>]) -> [Vec<int>]
pub fn kruskal<T: Clone + Ord>(edges: Vec<(T, T, int)>) -> Vec<(T, T, int)>
