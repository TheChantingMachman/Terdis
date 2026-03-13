use terdis::{execute, store::Store};

#[test]
fn echo_returns_message_verbatim() {
    let mut store = Store::new();
    let result = execute(&mut store, &["ECHO", "hello"]);
    assert_eq!(result, "hello");
}

#[test]
fn echo_returns_message_with_spaces() {
    let mut store = Store::new();
    let result = execute(&mut store, &["ECHO", "hello world"]);
    assert_eq!(result, "hello world");
}

#[test]
fn echo_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["ECHO"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'ECHO' command");
}

#[test]
fn echo_two_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["ECHO", "a", "b"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'ECHO' command");
}

#[test]
fn echo_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["echo", "test"]);
    assert_eq!(result, "test");
}

#[test]
fn echo_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Echo", "test"]);
    assert_eq!(result, "test");
}
