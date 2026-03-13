use terdis::{execute, Store};

#[test]
fn set_returns_ok() {
    let mut store = Store::new();
    assert_eq!(execute(&mut store, &["SET", "foo", "bar"]), "OK");
}

#[test]
fn set_overwrites_existing_key() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v1"]);
    let result = execute(&mut store, &["SET", "k", "v2"]);
    assert_eq!(result, "OK");
    assert_eq!(execute(&mut store, &["GET", "k"]), "v2");
}

#[test]
fn set_missing_value_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET", "key"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'SET' command"
    );
}

#[test]
fn set_no_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'SET' command"
    );
}

#[test]
fn set_too_many_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["SET", "key", "val", "extra"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'SET' command"
    );
}
