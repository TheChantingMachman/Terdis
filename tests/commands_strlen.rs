use terdis::{execute, store::Store};

#[test]
fn strlen_existing_key_returns_byte_length() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hello"]);
    let result = execute(&mut store, &["STRLEN", "k"]);
    assert_eq!(result, "5");
}

#[test]
fn strlen_missing_key_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["STRLEN", "nosuchkey"]);
    assert_eq!(result, "0");
}

#[test]
fn strlen_empty_string_value_returns_zero() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", ""]);
    let result = execute(&mut store, &["STRLEN", "k"]);
    assert_eq!(result, "0");
}

#[test]
fn strlen_returns_length_as_decimal_string() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "abcde"]);
    let result = execute(&mut store, &["STRLEN", "k"]);
    assert_eq!(result, "5");
}

#[test]
fn strlen_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["STRLEN"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'STRLEN' command");
}

#[test]
fn strlen_two_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["STRLEN", "k", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'STRLEN' command");
}

#[test]
fn strlen_case_insensitive_lowercase() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hi"]);
    let result = execute(&mut store, &["strlen", "k"]);
    assert_eq!(result, "2");
}

#[test]
fn strlen_after_append_reflects_new_length() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hello"]);
    execute(&mut store, &["APPEND", "k", " world"]);
    let result = execute(&mut store, &["STRLEN", "k"]);
    assert_eq!(result, "11");
}
