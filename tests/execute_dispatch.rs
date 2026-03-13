use terdis::{execute, store::Store};

#[test]
fn unknown_command_returns_error_with_name() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FOOBAR"]);
    assert_eq!(result, "(error) ERR unknown command 'FOOBAR'");
}

#[test]
fn unknown_command_preserves_case_in_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["foobar"]);
    assert_eq!(result, "(error) ERR unknown command 'foobar'");
}

#[test]
fn unknown_command_mixed_case_preserved() {
    let mut store = Store::new();
    let result = execute(&mut store, &["FooBar"]);
    assert_eq!(result, "(error) ERR unknown command 'FooBar'");
}
