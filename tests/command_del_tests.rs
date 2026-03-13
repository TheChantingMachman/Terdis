use terdis::{execute, Store};

#[test]
fn del_existing_key_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    assert_eq!(execute(&mut store, &["DEL", "k"]), "1");
}

#[test]
fn del_missing_key_returns_zero() {
    let mut store = Store::new();
    assert_eq!(execute(&mut store, &["DEL", "absent"]), "0");
}

#[test]
fn del_removes_key_from_store() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    execute(&mut store, &["DEL", "k"]);
    assert_eq!(execute(&mut store, &["GET", "k"]), "(nil)");
}

#[test]
fn del_multiple_keys_returns_count() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    // "c" does not exist
    assert_eq!(execute(&mut store, &["DEL", "a", "b", "c"]), "2");
}

#[test]
fn del_no_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DEL"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'DEL' command"
    );
}
