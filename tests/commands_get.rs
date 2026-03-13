use terdis::{execute, store::Store};

#[test]
fn get_existing_key_returns_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "hello"]);
    let result = execute(&mut store, &["GET", "k"]);
    assert_eq!(result, "hello");
}

#[test]
fn get_missing_key_returns_nil() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET", "nosuchkey"]);
    assert_eq!(result, "(nil)");
}

#[test]
fn get_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'GET' command");
}

#[test]
fn get_two_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET", "k", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'GET' command");
}

#[test]
fn get_after_del_returns_nil() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["DEL", "k"]);
    let result = execute(&mut store, &["GET", "k"]);
    assert_eq!(result, "(nil)");
}

#[test]
fn get_command_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["get", "k"]);
    assert_eq!(result, "v");
}
