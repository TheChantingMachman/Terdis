use terdis::{execute, store::Store};

#[test]
fn append_to_nonexistent_key_creates_it_and_returns_length() {
    let mut store = Store::new();
    let result = execute(&mut store, &["APPEND", "k", "hello"]);
    assert_eq!(result, "5");
}

#[test]
fn append_to_existing_key_concatenates_and_returns_length() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hello"]);
    let result = execute(&mut store, &["APPEND", "k", " world"]);
    assert_eq!(result, "11");
}

#[test]
fn append_value_is_retrievable_after_append() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hello"]);
    execute(&mut store, &["APPEND", "k", " world"]);
    let result = execute(&mut store, &["GET", "k"]);
    assert_eq!(result, "hello world");
}

#[test]
fn append_to_nonexistent_key_value_is_retrievable() {
    let mut store = Store::new();
    execute(&mut store, &["APPEND", "newkey", "abc"]);
    let result = execute(&mut store, &["GET", "newkey"]);
    assert_eq!(result, "abc");
}

#[test]
fn append_returns_byte_length_not_char_count() {
    // ASCII only — byte length == char count for basic case
    let mut store = Store::new();
    let result = execute(&mut store, &["APPEND", "k", "abc"]);
    assert_eq!(result, "3");
}

#[test]
fn append_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["APPEND"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'APPEND' command");
}

#[test]
fn append_one_arg_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["APPEND", "key"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'APPEND' command");
}

#[test]
fn append_three_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["APPEND", "k", "v", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'APPEND' command");
}

#[test]
fn append_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["append", "k", "hello"]);
    assert_eq!(result, "5");
}

#[test]
fn append_multiple_times_accumulates() {
    let mut store = Store::new();
    execute(&mut store, &["APPEND", "k", "foo"]);
    execute(&mut store, &["APPEND", "k", "bar"]);
    let result = execute(&mut store, &["APPEND", "k", "baz"]);
    assert_eq!(result, "9");
    assert_eq!(execute(&mut store, &["GET", "k"]), "foobarbaz");
}
