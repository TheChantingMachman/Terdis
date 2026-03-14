use terdis::{execute, store::Store};

// --- arity ---

#[test]
fn flushdb_one_arg_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FLUSHDB", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'FLUSHDB' command");
}

#[test]
fn flushdb_two_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FLUSHDB", "a", "b"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'FLUSHDB' command");
}

// --- success ---

#[test]
fn flushdb_returns_ok() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FLUSHDB"]);
    assert_eq!(result, "OK");
}

#[test]
fn flushdb_empty_store_returns_ok() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FLUSHDB"]);
    assert_eq!(result, "OK");
}

// --- clears all keys ---

#[test]
fn flushdb_removes_all_keys() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    execute(&mut store, &["SET", "c", "3"]);
    execute(&mut store, &["FLUSHDB"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "0");
}

#[test]
fn flushdb_keys_not_accessible_after_flush() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "mykey", "myval"]);
    execute(&mut store, &["FLUSHDB"]);
    let result = execute(&mut store, &["GET", "mykey"]);
    assert_eq!(result, "(nil)");
}

#[test]
fn flushdb_exists_returns_zero_after_flush() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["FLUSHDB"]);
    let result = execute(&mut store, &["EXISTS", "k"]);
    assert_eq!(result, "0");
}

#[test]
fn flushdb_keys_star_returns_empty_after_flush() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "x", "1"]);
    execute(&mut store, &["SET", "y", "2"]);
    execute(&mut store, &["FLUSHDB"]);
    let result = execute(&mut store, &["KEYS", "*"]);
    assert_eq!(result, "(empty)");
}

// --- store is usable after flush ---

#[test]
fn flushdb_store_accepts_new_keys_after_flush() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "old", "val"]);
    execute(&mut store, &["FLUSHDB"]);
    execute(&mut store, &["SET", "new", "fresh"]);
    let result = execute(&mut store, &["GET", "new"]);
    assert_eq!(result, "fresh");
}

#[test]
fn flushdb_dbsize_is_correct_after_partial_refill() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    execute(&mut store, &["FLUSHDB"]);
    execute(&mut store, &["SET", "c", "3"]);
    let result = execute(&mut store, &["DBSIZE"]);
    assert_eq!(result, "1");
}

// --- case insensitive command ---

#[test]
fn flushdb_command_name_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["flushdb"]);
    assert_eq!(result, "OK");
}
