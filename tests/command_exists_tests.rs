use terdis::{execute, Store};

#[test]
fn exists_present_key_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    assert_eq!(execute(&mut store, &["EXISTS", "k"]), "1");
}

#[test]
fn exists_absent_key_returns_zero() {
    let mut store = Store::new();
    assert_eq!(execute(&mut store, &["EXISTS", "missing"]), "0");
}

#[test]
fn exists_multiple_keys_returns_count() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "a", "1"]);
    execute(&mut store, &["SET", "b", "2"]);
    // "c" does not exist
    assert_eq!(execute(&mut store, &["EXISTS", "a", "b", "c"]), "2");
}

#[test]
fn exists_duplicate_keys_counts_each_occurrence() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    // same key passed twice — each occurrence is counted
    assert_eq!(execute(&mut store, &["EXISTS", "k", "k"]), "2");
}

#[test]
fn exists_no_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["EXISTS"]);
    assert_eq!(
        result,
        "(error) ERR wrong number of arguments for 'EXISTS' command"
    );
}
