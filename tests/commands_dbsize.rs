use terdis::{execute, store::Store};

#[test]
fn dbsize_empty_store_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "0");
}

#[test]
fn dbsize_after_one_set_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "1");
}

#[test]
fn dbsize_reflects_multiple_keys() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    execute(&mut store, &["SET", "c", "3"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "3");
}

#[test]
fn dbsize_decrements_after_del() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    execute(&mut store, &["DEL", "a"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "1");
}

#[test]
fn dbsize_overwrite_does_not_increase_count() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v1"]);
    execute(&mut store, &["SET", "k", "v2"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "1");
}

#[test]
fn dbsize_with_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DBSIZE", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DBSIZE' command");
}

#[test]
fn dbsize_command_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["dbsize"]);
    assert_eq!(result, "1");
}
