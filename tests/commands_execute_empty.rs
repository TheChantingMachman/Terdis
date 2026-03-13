use terdis::{execute, store::Store};

#[test]
fn empty_args_returns_no_command_provided_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &[]);
    assert_eq!(result, "(error) ERR no command provided");
}
