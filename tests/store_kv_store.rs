use terdis::store::KvStore;

#[test]
fn new_store_is_empty() {
    let store = KvStore::new();
    assert_eq!(store.get("any_key"), None);
}

#[test]
fn set_and_get_value() {
    let mut store = KvStore::new();
    store.set("key".to_string(), "value".to_string());
    assert_eq!(store.get("key"), Some(&"value".to_string()));
}

#[test]
fn get_missing_key_returns_none() {
    let store = KvStore::new();
    assert_eq!(store.get("missing"), None);
}

#[test]
fn set_duplicate_key_overwrites_previous_value() {
    let mut store = KvStore::new();
    store.set("key".to_string(), "first".to_string());
    store.set("key".to_string(), "second".to_string());
    assert_eq!(store.get("key"), Some(&"second".to_string()));
}

#[test]
fn keys_are_case_sensitive() {
    let mut store = KvStore::new();
    store.set("Key".to_string(), "upper".to_string());
    store.set("key".to_string(), "lower".to_string());
    assert_eq!(store.get("Key"), Some(&"upper".to_string()));
    assert_eq!(store.get("key"), Some(&"lower".to_string()));
    assert_eq!(store.get("KEY"), None);
}

#[test]
fn remove_existing_key_returns_true() {
    let mut store = KvStore::new();
    store.set("key".to_string(), "value".to_string());
    assert!(store.remove("key"));
}

#[test]
fn remove_existing_key_makes_it_inaccessible() {
    let mut store = KvStore::new();
    store.set("key".to_string(), "value".to_string());
    store.remove("key");
    assert_eq!(store.get("key"), None);
}

#[test]
fn remove_missing_key_returns_false() {
    let mut store = KvStore::new();
    assert!(!store.remove("nonexistent"));
}

#[test]
fn multiple_distinct_keys_coexist() {
    let mut store = KvStore::new();
    store.set("a".to_string(), "1".to_string());
    store.set("b".to_string(), "2".to_string());
    store.set("c".to_string(), "3".to_string());
    assert_eq!(store.get("a"), Some(&"1".to_string()));
    assert_eq!(store.get("b"), Some(&"2".to_string()));
    assert_eq!(store.get("c"), Some(&"3".to_string()));
}

#[test]
fn remove_one_key_does_not_affect_others() {
    let mut store = KvStore::new();
    store.set("a".to_string(), "1".to_string());
    store.set("b".to_string(), "2".to_string());
    store.remove("a");
    assert_eq!(store.get("a"), None);
    assert_eq!(store.get("b"), Some(&"2".to_string()));
}

#[test]
fn set_after_remove_restores_key() {
    let mut store = KvStore::new();
    store.set("key".to_string(), "original".to_string());
    store.remove("key");
    store.set("key".to_string(), "restored".to_string());
    assert_eq!(store.get("key"), Some(&"restored".to_string()));
}

#[test]
fn arbitrary_binary_safe_strings_as_keys_and_values() {
    let mut store = KvStore::new();
    let key = "key with spaces and \t tabs".to_string();
    let value = "value\nwith\nnewlines".to_string();
    store.set(key.clone(), value.clone());
    assert_eq!(store.get(&key), Some(&value));
}
