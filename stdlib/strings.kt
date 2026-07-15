// ─── Ketamine Standard Library: String Utilities ─────────────────────────────

impl str {
    /// Length in bytes.
    pub fn len(self) -> int

    /// Length in characters (Unicode codepoints).
    pub fn char_len(self) -> int

    /// Check if string is empty.
    pub fn is_empty(self) -> bool

    /// Get byte at index.
    pub fn byte_at(self, index: int) -> u8

    /// Get character at index (Unicode codepoint).
    pub fn char_at(self, index: int) -> char

    /// Get substring in byte range [start, end).
    pub fn slice(self, start: int, end: int) -> str

    /// Get substring from start to end of string.
    pub fn from(self, start: int) -> str

    /// Find first occurrence of a substring. Returns -1 if not found.
    pub fn find(self, pattern: str) -> int

    /// Find last occurrence of a substring.
    pub fn rfind(self, pattern: str) -> int

    /// Check if string starts with pattern.
    pub fn starts_with(self, pattern: str) -> bool

    /// Check if string ends with pattern.
    pub fn ends_with(self, pattern: str) -> bool

    /// Split string by delimiter.
    pub fn split(self, delimiter: str) -> Vec<str>

    /// Split string by whitespace.
    pub fn split_whitespace(self) -> Vec<str>

    /// Split string into lines.
    pub fn lines(self) -> Vec<str>

    /// Trim whitespace from both ends.
    pub fn trim(self) -> str

    /// Trim whitespace from start.
    pub fn trim_start(self) -> str

    /// Trim whitespace from end.
    pub fn trim_end(self) -> str

    /// Convert to uppercase.
    pub fn to_upper(self) -> str

    /// Convert to lowercase.
    pub fn to_lower(self) -> str

    /// Repeat string n times.
    pub fn repeat(self, n: int) -> str

    /// Replace all occurrences of a pattern.
    pub fn replace(self, from: str, to: str) -> str

    /// Check if string matches a regex/glob pattern.
    pub fn matches(self, pattern: str) -> bool

    /// Count occurrences of a substring.
    pub fn count(self, pattern: str) -> int

    /// Pad string to a minimum width.
    pub fn pad_start(self, width: int, pad: char) -> str
    pub fn pad_end(self, width: int, pad: char) -> str

    /// Reverse the string.
    pub fn reverse(self) -> str

    /// Check if string contains substring.
    pub fn contains(self, pattern: str) -> bool

    /// Convert to UTF-8 bytes.
    pub fn to_bytes(self) -> [u8]

    /// Hash the string.
    pub fn hash(self) -> int

    /// Compare strings (for sorting).
    pub fn cmp(self, other: str) -> int

    /// Concatenate strings.
    pub fn concat(self, other: str) -> str

    /// Capitalize the first character.
    pub fn capitalize(self) -> str

    /// Convert to title case.
    pub fn title_case(self) -> str

    /// Convert to snake_case.
    pub fn to_snake_case(self) -> str

    /// Convert to camelCase.
    pub fn to_camel_case(self) -> str

    /// Convert to PascalCase.
    pub fn to_pascal_case(self) -> str

    /// Check if string is ASCII.
    pub fn is_ascii(self) -> bool

    /// Check if string is numeric.
    pub fn is_numeric(self) -> bool

    /// Check if string is alphanumeric.
    pub fn is_alphanumeric(self) -> bool
}

/// Format a string (similar to Python f-strings / Rust format!).
pub fn format(fmt: str, args: ...) -> str

/// Join a list of strings with a separator.
pub fn join(parts: Vec<str>, sep: str) -> str

/// Concatenate a list of strings.
pub fn concat(parts: Vec<str>) -> str

// ─── Regex (if supported by backend) ──────────────────────────────────────────

pub mod regex {
    pub struct Regex { /* opaque */ }

    impl Regex {
        pub fn compile(pattern: str) -> Regex?
        pub fn is_match(self, text: str) -> bool
        pub fn find(self, text: str) -> str?
        pub fn find_all(self, text: str) -> Vec<str>
        pub fn capture(self, text: str) -> Vec<str>?
        pub fn replace(self, text: str, replacement: str) -> str
        pub fn split(self, text: str) -> Vec<str>
    }
}

// ─── Unicode ──────────────────────────────────────────────────────────────────

pub fn unicode_category(cp: char) -> str
pub fn is_alpha(cp: char) -> bool
pub fn is_digit(cp: char) -> bool
pub fn is_alphanumeric(cp: char) -> bool
pub fn is_whitespace(cp: char) -> bool
pub fn is_upper(cp: char) -> bool
pub fn is_lower(cp: char) -> bool
pub fn to_upper(cp: char) -> char
pub fn to_lower(cp: char) -> char
pub fn unicode_name(cp: char) -> str?
