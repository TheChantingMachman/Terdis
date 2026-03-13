use terdis::{execute, Store};

#[test]
fn get_existing_key_returns_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "hello", "world"]);
    assert_eq!(execute(&mut store, &["GET", "hello"]), "world");
}

#[test]
fn get_missing_key_returns_nil() {
    let mut store = Store::new();
    assert_eq!(execute(&mut store, &["GET", "absent"]), "(nil)");
}

#[test]
fn get_no_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'GET' command"
    );
}

#[test]
fn get_too_many_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET", "k1", "k2"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'GET' command"
    );
}
