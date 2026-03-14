use terdis::{execute, store::Store};

// --- arity ---

#[test]
fn keys_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["KEYS"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'KEYS' command");
}

#[test]
fn keys_two_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["KEYS", "*", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'KEYS' command");
}

// --- empty store ---

#[test]
fn keys_empty_store_returns_empty_sentinel() {
    let mut store = Store::new();
    let result = execute(&mut store, &["KEYS", "*"]);
    assert_eq!(result, "(empty)");
}

#[test]
fn keys_no_match_returns_empty_sentinel() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "hello", "world"]);
    let result = execute(&mut store, &["KEYS", "xyz*"]);
    assert_eq!(result, "(empty)");
}

// --- wildcard * ---

#[test]
fn keys_star_matches_all_keys() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    execute(&mut store, &["SET", "c", "3"]);
    let result = execute(&mut store, &["KEYS", "*"]);
    assert_eq!(result, "a\nb\nc");
}

#[test]
fn keys_star_matches_any_sequence_including_empty() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "foo", "1"]);
    execute(&mut store, &["SET", "foobar", "2"]);
    execute(&mut store, &["SET", "bar", "3"]);
    let result = execute(&mut store, &["KEYS", "foo*"]);
    // "foo" and "foobar" match; sorted lexicographically
    assert_eq!(result, "foo\nfoobar");
}

#[test]
fn keys_star_at_start_matches_suffix() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "hello", "1"]);
    execute(&mut store, &["SET", "world", "2"]);
    execute(&mut store, &["SET", "lo", "3"]);
    let result = execute(&mut store, &["KEYS", "*lo"]);
    // "hello" ends with "lo", "lo" matches too
    assert_eq!(result, "hello\nlo");
}

#[test]
fn keys_star_matches_empty_string_prefix() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "v"]);
    let result = execute(&mut store, &["KEYS", "*key"]);
    assert_eq!(result, "key");
}

// --- wildcard ? ---

#[test]
fn keys_question_mark_matches_exactly_one_char() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "ab", "1"]);
    execute(&mut store, &["SET", "ac", "2"]);
    execute(&mut store, &["SET", "abc", "3"]);
    let result = execute(&mut store, &["KEYS", "a?"]);
    // "ab" and "ac" match (exactly one char after 'a'); "abc" does not
    assert_eq!(result, "ab\nac");
}

#[test]
fn keys_question_mark_does_not_match_zero_chars() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "ab", "2"]);
    let result = execute(&mut store, &["KEYS", "a?"]);
    // "a" does not match "a?" (? requires exactly one char)
    assert_eq!(result, "ab");
}

#[test]
fn keys_multiple_question_marks() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "abc", "1"]);
    execute(&mut store, &["SET", "aXc", "2"]);
    execute(&mut store, &["SET", "abcd", "3"]);
    let result = execute(&mut store, &["KEYS", "a?c"]);
    // "abc" and "aXc" match; "abcd" does not
    assert_eq!(result, "aXc\nabc");
}

// --- literal matching ---

#[test]
fn keys_exact_pattern_matches_only_that_key() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "foo", "1"]);
    execute(&mut store, &["SET", "bar", "2"]);
    let result = execute(&mut store, &["KEYS", "foo"]);
    assert_eq!(result, "foo");
}

#[test]
fn keys_bracket_chars_are_literals() {
    // '[' is not a supported glob char — treated as literal
    let mut store = Store::new();
    execute(&mut store, &["SET", "[a]", "1"]);
    execute(&mut store, &["SET", "a", "2"]);
    let result = execute(&mut store, &["KEYS", "[a]"]);
    assert_eq!(result, "[a]");
}

#[test]
fn keys_dot_is_literal_not_regex_wildcard() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a.b", "1"]);
    execute(&mut store, &["SET", "axb", "2"]);
    let result = execute(&mut store, &["KEYS", "a.b"]);
    // '.' is literal — only "a.b" matches, not "axb"
    assert_eq!(result, "a.b");
}

// --- lexicographic ordering ---

#[test]
fn keys_results_are_sorted_lexicographically() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "z", "1"]);
    execute(&mut store, &["SET", "a", "2"]);
    execute(&mut store, &["SET", "m", "3"]);
    let result = execute(&mut store, &["KEYS", "*"]);
    assert_eq!(result, "a\nm\nz");
}

#[test]
fn keys_no_trailing_newline() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "x", "1"]);
    execute(&mut store, &["SET", "y", "2"]);
    let result = execute(&mut store, &["KEYS", "*"]);
    assert!(!result.ends_with('\n'), "result must not have a trailing newline");
    assert_eq!(result, "x\ny");
}

// --- single match ---

#[test]
fn keys_single_match_returns_key_without_newline() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "only", "v"]);
    let result = execute(&mut store, &["KEYS", "*"]);
    assert_eq!(result, "only");
}

// --- case sensitivity ---

#[test]
fn keys_pattern_is_case_sensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "Foo", "1"]);
    execute(&mut store, &["SET", "foo", "2"]);
    let result = execute(&mut store, &["KEYS", "foo"]);
    assert_eq!(result, "foo");
}

// --- case insensitive command ---

#[test]
fn keys_command_name_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["keys", "*"]);
    assert_eq!(result, "k");
}
