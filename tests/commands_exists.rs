use terdis::{execute, store::Store};

#[test]
fn exists_single_present_key_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["EXISTS", "k"]);
    assert_eq!(result, "1");
}

#[test]
fn exists_single_absent_key_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["EXISTS", "missing"]);
    assert_eq!(result, "0");
}

#[test]
fn exists_multiple_keys_returns_count_of_existing() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    let result = execute(&mut store, &["EXISTS", "a", "b", "missing"]);
    assert_eq!(result, "2");
}

#[test]
fn exists_duplicate_key_counted_multiple_times() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["EXISTS", "k", "k", "k"]);
    assert_eq!(result, "3");
}

#[test]
fn exists_duplicate_absent_key_returns_zero() {
    let mut store = Store::new();
    let result = execute(&mut store, &["EXISTS", "x", "x"]);
    assert_eq!(result, "0");
}

#[test]
fn exists_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["EXISTS"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'EXISTS' command");
}

#[test]
fn exists_returns_zero_after_del() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["DEL", "k"]);
    let result = execute(&mut store, &["EXISTS", "k"]);
    assert_eq!(result, "0");
}

#[test]
fn exists_command_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["exists", "k"]);
    assert_eq!(result, "1");
}
