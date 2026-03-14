use terdis::{execute, store::Store};

// --- arity ---

#[test]
fn rename_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAME"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAME' command");
}

#[test]
fn rename_one_arg_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAME", "key"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAME' command");
}

#[test]
fn rename_three_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAME", "k", "nk", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAME' command");
}

// --- key does not exist ---

#[test]
fn rename_missing_key_returns_no_such_key_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAME", "ghost", "newkey"]);
    assert_eq!(result, "(error) ERR no such key");
}

// --- success ---

#[test]
fn rename_returns_ok_on_success() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "hello"]);
    let result = execute(&mut store, &["RENAME", "src", "dst"]);
    assert_eq!(result, "OK");
}

#[test]
fn rename_new_key_holds_old_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "hello"]);
    execute(&mut store, &["RENAME", "src", "dst"]);
    let result = execute(&mut store, &["GET", "dst"]);
    assert_eq!(result, "hello");
}

#[test]
fn rename_old_key_no_longer_exists() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "hello"]);
    execute(&mut store, &["RENAME", "src", "dst"]);
    let result = execute(&mut store, &["EXISTS", "src"]);
    assert_eq!(result, "0");
}

// --- overwrite existing newkey ---

#[test]
fn rename_overwrites_existing_newkey() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "new_val"]);
    execute(&mut store, &["SET", "dst", "old_val"]);
    execute(&mut store, &["RENAME", "src", "dst"]);
    let result = execute(&mut store, &["GET", "dst"]);
    assert_eq!(result, "new_val");
}

#[test]
fn rename_overwrite_removes_src() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "val"]);
    execute(&mut store, &["SET", "dst", "other"]);
    execute(&mut store, &["RENAME", "src", "dst"]);
    let result = execute(&mut store, &["EXISTS", "src"]);
    assert_eq!(result, "0");
}

#[test]
fn rename_overwrite_returns_ok() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "v1"]);
    execute(&mut store, &["SET", "dst", "v2"]);
    let result = execute(&mut store, &["RENAME", "src", "dst"]);
    assert_eq!(result, "OK");
}

// --- key == newkey (resolved assumption) ---

#[test]
fn rename_key_equals_newkey_returns_ok() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "same", "value"]);
    let result = execute(&mut store, &["RENAME", "same", "same"]);
    assert_eq!(result, "OK");
}

#[test]
fn rename_key_equals_newkey_value_unchanged() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "same", "value"]);
    execute(&mut store, &["RENAME", "same", "same"]);
    let result = execute(&mut store, &["GET", "same"]);
    assert_eq!(result, "value");
}

// --- case insensitive command ---

#[test]
fn rename_command_name_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["rename", "k", "k2"]);
    assert_eq!(result, "OK");
}
