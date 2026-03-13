use terdis::{execute, store::Store};

#[test]
fn mset_single_pair_returns_ok() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MSET", "k1", "v1"]);
    assert_eq!(result, "OK");
}

#[test]
fn mset_multiple_pairs_returns_ok() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MSET", "k1", "v1", "k2", "v2", "k3", "v3"]);
    assert_eq!(result, "OK");
}

#[test]
fn mset_stores_all_values_retrievable_by_get() {
    let mut store = Store::new();
    execute(&mut store, &["MSET", "k1", "v1", "k2", "v2"]);
    assert_eq!(execute(&mut store, &["GET", "k1"]), "v1");
    assert_eq!(execute(&mut store, &["GET", "k2"]), "v2");
}

#[test]
fn mset_overwrites_existing_key() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "old"]);
    execute(&mut store, &["MSET", "k", "new"]);
    assert_eq!(execute(&mut store, &["GET", "k"]), "new");
}

#[test]
fn mset_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MSET"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'MSET' command");
}

#[test]
fn mset_odd_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MSET", "k1", "v1", "k2"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'MSET' command");
}

#[test]
fn mset_single_key_no_value_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["MSET", "onlykey"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'MSET' command");
}

#[test]
fn mset_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["mset", "k", "v"]);
    assert_eq!(result, "OK");
}

#[test]
fn mset_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Mset", "k", "v"]);
    assert_eq!(result, "OK");
}
