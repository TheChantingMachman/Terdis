use terdis::store::Store;

#[test]
fn new_store_is_empty() {
    let store = Store::new();
    assert_eq!(store.get("key"), None);
}

#[test]
fn set_and_get_roundtrip() {
    let mut store = Store::new();
    store.set("hello".to_string(), "world".to_string());
    assert_eq!(store.get("hello"), Some(&"world".to_string()));
}

#[test]
fn keys_are_case_sensitive() {
    let mut store = Store::new();
    store.set("Key".to_string(), "upper".to_string());
    store.set("key".to_string(), "lower".to_string());
    assert_eq!(store.get("Key"), Some(&"upper".to_string()));
    assert_eq!(store.get("key"), Some(&"lower".to_string()));
}

#[test]
fn set_overwrites_duplicate_key() {
    let mut store = Store::new();
    store.set("k".to_string(), "first".to_string());
    store.set("k".to_string(), "second".to_string());
    assert_eq!(store.get("k"), Some(&"second".to_string()));
}

#[test]
fn del_existing_key_returns_true() {
    let mut store = Store::new();
    store.set("k".to_string(), "v".to_string());
    assert!(store.del("k"));
}

#[test]
fn del_existing_key_removes_it() {
    let mut store = Store::new();
    store.set("k".to_string(), "v".to_string());
    store.del("k");
    assert_eq!(store.get("k"), None);
}

#[test]
fn del_missing_key_returns_false() {
    let mut store = Store::new();
    assert!(!store.del("nonexistent"));
}

#[test]
fn exists_returns_true_for_present_key() {
    let mut store = Store::new();
    store.set("k".to_string(), "v".to_string());
    assert!(store.exists("k"));
}

#[test]
fn exists_returns_false_for_absent_key() {
    let store = Store::new();
    assert!(!store.exists("missing"));
}

#[test]
fn exists_returns_false_after_del() {
    let mut store = Store::new();
    store.set("k".to_string(), "v".to_string());
    store.del("k");
    assert!(!store.exists("k"));
}
