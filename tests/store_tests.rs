use terdis::{execute, Store};

#[test]
fn new_store_is_empty() {
    let mut store = Store::new();
    let result = execute(&mut store, &["GET", "missing"]);
    assert_eq!(result, "(nil)");
}

#[test]
fn duplicate_set_overwrites() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "first"]);
    execute(&mut store, &["SET", "key", "second"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "second");
}

#[test]
fn keys_are_case_sensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "lower"]);
    execute(&mut store, &["SET", "KEY", "upper"]);
    assert_eq!(execute(&mut store, &["GET", "key"]), "lower");
    assert_eq!(execute(&mut store, &["GET", "KEY"]), "upper");
}
