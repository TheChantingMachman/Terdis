use terdis::{execute, store::Store};

#[test]
fn mget_single_existing_key_returns_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k1", "val1"]);
    let result = execute(&mut store, &["MGET", "k1"]);
    assert_eq!(result, "val1");
}

#[test]
fn mget_single_missing_key_returns_nil() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MGET", "nosuchkey"]);
    assert_eq!(result, "(nil)");
}

#[test]
fn mget_multiple_existing_keys_joined_by_newline() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k1", "val1"]);
    execute(&mut store, &["SET", "k2", "val2"]);
    execute(&mut store, &["SET", "k3", "val3"]);
    let result = execute(&mut store, &["MGET", "k1", "k2", "k3"]);
    assert_eq!(result, "val1\nval2\nval3");
}

#[test]
fn mget_missing_key_in_middle_produces_nil_at_that_position() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k1", "val1"]);
    execute(&mut store, &["SET", "k3", "val3"]);
    let result = execute(&mut store, &["MGET", "k1", "k2", "k3"]);
    assert_eq!(result, "val1\n(nil)\nval3");
}

#[test]
fn mget_no_trailing_newline() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k1", "val1"]);
    execute(&mut store, &["SET", "k3", "val3"]);
    let result = execute(&mut store, &["MGET", "k1", "k2", "k3"]);
    assert!(!result.ends_with('\n'));
}

#[test]
fn mget_all_missing_keys_returns_all_nil() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MGET", "a", "b", "c"]);
    assert_eq!(result, "(nil)\n(nil)\n(nil)");
}

#[test]
fn mget_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MGET"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'MGET' command");
}

#[test]
fn mget_case_insensitive_lowercase() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["mget", "k"]);
    assert_eq!(result, "v");
}

#[test]
fn mget_keys_are_case_sensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "Key", "upper"]);
    let result = execute(&mut store, &["MGET", "Key", "key"]);
    assert_eq!(result, "upper\n(nil)");
}

#[test]
fn mget_two_keys_one_missing_at_end() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k1", "hello"]);
    let result = execute(&mut store, &["MGET", "k1", "missing"]);
    assert_eq!(result, "hello\n(nil)");
}
