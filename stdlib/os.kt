// ─── Ketamine Standard Library: OS & System ───────────────────────────────────

// ─── Process ──────────────────────────────────────────────────────────────────

pub struct Process { /* opaque */ }

impl Process {
    pub fn spawn(cmd: str, args: [str]) -> Process?
    pub fn spawn_with(cmd: str, args: [str], env: HashMap<str, str>, dir: str) -> Process?

    pub fn wait(self) -> int
    pub fn kill(self) -> bool
    pub fn is_running(self) -> bool
    pub fn pid(self) -> int
    pub fn stdin(self) -> File?
    pub fn stdout(self) -> File?
    pub fn stderr(self) -> File?
}

/// Execute a command and return its output.
pub fn exec(cmd: str, args: [str]) -> (int, str, str)?

/// Execute a command with input and return output.
pub fn exec_with_stdin(cmd: str, args: [str], input: str) -> (int, str, str)?

// ─── Signals ──────────────────────────────────────────────────────────────────

pub fn signal(sig: int, handler: fn(int) -> void) -> bool
pub fn kill(pid: int, sig: int) -> bool

pub const SIGINT:  int = 2
pub const SIGTERM: int = 15
pub const SIGKILL: int = 9
pub const SIGUSR1: int = 10
pub const SIGUSR2: int = 12

// ─── System Information ───────────────────────────────────────────────────────

pub fn hostname() -> str
pub fn os_name() -> str
pub fn os_version() -> str
pub fn cpu_count() -> int
pub fn process_id() -> int
pub fn parent_process_id() -> int
pub fn uptime() -> i64
pub fn load_avg() -> (float, float, float)
pub fn memory_info() -> (i64, i64, i64)  // total, used, free

// ─── Threading ────────────────────────────────────────────────────────────────

pub fn spawn_thread<T>(f: fn() -> T) -> Thread<T>?
pub fn sleep_ms(ms: i64)

pub struct Thread<T> { /* opaque */ }

impl<T> Thread<T> {
    pub fn join(self) -> T
    pub fn detach(self)
    pub fn id(self) -> i64
}

pub struct Mutex<T> {
    inner: /* opaque */
    data: T
}

impl<T> Mutex<T> {
    pub fn new(val: T) -> Mutex<T>
    pub fn lock(self) -> &mut T
    pub fn try_lock(self) -> &mut T?
}

pub struct RwLock<T> { /* opaque */ }

impl<T> RwLock<T> {
    pub fn new(val: T) -> RwLock<T>
    pub fn read(self) -> &T
    pub fn write(self) -> &mut T
    pub fn try_read(self) -> &T?
    pub fn try_write(self) -> &mut T?
}

pub struct Condvar { /* opaque */ }

impl Condvar {
    pub fn new() -> Condvar
    pub fn wait(self, m: &Mutex)
    pub fn notify_one(self)
    pub fn notify_all(self)
}

pub struct AtomicInt { /* opaque */ }

impl AtomicInt {
    pub fn new(val: int) -> AtomicInt
    pub fn load(self) -> int
    pub fn store(self, val: int)
    pub fn add(self, val: int) -> int
    pub fn sub(self, val: int) -> int
    pub fn swap(self, val: int) -> int
    pub fn compare_exchange(self, expected: int, desired: int) -> bool
}

// ─── Filesystem Events ────────────────────────────────────────────────────────

pub struct FsWatcher { /* opaque */ }

impl FsWatcher {
    pub fn watch(path: str, recursive: bool) -> FsWatcher?
    pub fn poll(self) -> [FsEvent]
    pub fn close(self)
}

pub enum FsEvent {
    Created(str),
    Modified(str),
    Deleted(str),
    Renamed(str, str)
}
