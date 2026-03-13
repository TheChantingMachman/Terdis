use terdis::{execute, store::Store};

#[test]
fn set_returns_ok() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET", "key", "value"]);
    assert_eq!(result, "OK");
}

#[test]
fn set_stores_value_retrievable_by_get() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "mykey", "myval"]);
    let result = execute(&mut store, &["GET", "mykey"]);
    assert_eq!(result, "myval");
}

#[test]
fn set_overwrites_existing_key() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "first"]);
    let result = execute(&mut store, &["SET", "k", "second"]);
    assert_eq!(result, "OK");
    assert_eq!(execute(&mut store, &["GET", "k"]), "second");
}

#[test]
fn set_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'SET' command");
}

#[test]
fn set_one_arg_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET", "onlykey"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'SET' command");
}

#[test]
fn set_three_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET", "k", "v", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'SET' command");
}

#[test]
fn set_command_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["set", "k", "v"]);
    assert_eq!(result, "OK");
}

#[test]
fn set_command_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Set", "k", "v"]);
    assert_eq!(result, "OK");
}
