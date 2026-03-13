use terdis::{execute, store::Store};

#[test]
fn ping_no_args_returns_pong() {
    let mut store = Store::new();
    let result = execute(&mut store, &["PING"]);
    assert_eq!(result, "PONG");
}

#[test]
fn ping_one_arg_returns_arg_verbatim() {
    let mut store = Store::new();
    let result = execute(&mut store, &["PING", "hello"]);
    assert_eq!(result, "hello");
}

#[test]
fn ping_one_arg_with_spaces_returns_arg_verbatim() {
    let mut store = Store::new();
    let result = execute(&mut store, &["PING", "hello world"]);
    assert_eq!(result, "hello world");
}

#[test]
fn ping_two_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["PING", "a", "b"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'PING' command");
}

#[test]
fn ping_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["ping"]);
    assert_eq!(result, "PONG");
}

#[test]
fn ping_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Ping", "msg"]);
    assert_eq!(result, "msg");
}
