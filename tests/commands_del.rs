use terdis::{execute, store::Store};

#[test]
fn del_single_existing_key_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["DEL", "k"]);
    assert_eq!(result, "1");
}

#[test]
fn del_single_missing_key_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DEL", "nosuchkey"]);
    assert_eq!(result, "0");
}

#[test]
fn del_multiple_keys_returns_count_of_deleted() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    let result = execute(&mut store, &["DEL", "a", "b", "missing"]);
    assert_eq!(result, "2");
}

#[test]
fn del_removes_key_from_store() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["DEL", "k"]);
    assert_eq!(execute(&mut store, &["GET", "k"]), "(nil)");
}

#[test]
fn del_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DEL"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DEL' command");
}

#[test]
fn del_all_missing_keys_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DEL", "x", "y", "z"]);
    assert_eq!(result, "0");
}

#[test]
fn del_command_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["del", "k"]);
    assert_eq!(result, "1");
}
