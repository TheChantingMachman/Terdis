use terdis::{execute, store::Store};

#[test]
fn type_existing_key_returns_string() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["TYPE", "k"]);
    assert_eq!(result, "string");
}

#[test]
fn type_missing_key_returns_none() {
    let mut store = Store::new();
    let result = execute(&mut store, &["TYPE", "nosuchkey"]);
    assert_eq!(result, "none");
}

#[test]
fn type_after_del_returns_none() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["DEL", "k"]);
    let result = execute(&mut store, &["TYPE", "k"]);
    assert_eq!(result, "none");
}

#[test]
fn type_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["TYPE"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'TYPE' command");
}

#[test]
fn type_two_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["TYPE", "a", "b"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'TYPE' command");
}

#[test]
fn type_command_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["type", "k"]);
    assert_eq!(result, "string");
}
