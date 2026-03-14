use terdis::{execute, store::Store};

// --- arity ---

#[test]
fn renamenx_zero_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAMENX"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAMENX' command");
}

#[test]
fn renamenx_one_arg_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAMENX", "key"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAMENX' command");
}

#[test]
fn renamenx_three_args_returns_arity_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAMENX", "k", "nk", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'RENAMENX' command");
}

// --- key does not exist ---

#[test]
fn renamenx_missing_key_returns_no_such_key_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["RENAMENX", "ghost", "newkey"]);
    assert_eq!(result, "(error) ERR no such key");
}

// --- newkey does not exist: rename succeeds ---

#[test]
fn renamenx_newkey_absent_returns_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "val"]);
    let result = execute(&mut store, &["RENAMENX", "src", "dst"]);
    assert_eq!(result, "1");
}

#[test]
fn renamenx_newkey_absent_dst_holds_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "hello"]);
    execute(&mut store, &["RENAMENX", "src", "dst"]);
    let result = execute(&mut store, &["GET", "dst"]);
    assert_eq!(result, "hello");
}

#[test]
fn renamenx_newkey_absent_src_removed() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "hello"]);
    execute(&mut store, &["RENAMENX", "src", "dst"]);
    let result = execute(&mut store, &["EXISTS", "src"]);
    assert_eq!(result, "0");
}

// --- newkey already exists: no rename ---

#[test]
fn renamenx_newkey_present_returns_zero() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "new_val"]);
    execute(&mut store, &["SET", "dst", "old_val"]);
    let result = execute(&mut store, &["RENAMENX", "src", "dst"]);
    assert_eq!(result, "0");
}

#[test]
fn renamenx_newkey_present_dst_value_unchanged() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "new_val"]);
    execute(&mut store, &["SET", "dst", "old_val"]);
    execute(&mut store, &["RENAMENX", "src", "dst"]);
    let result = execute(&mut store, &["GET", "dst"]);
    assert_eq!(result, "old_val");
}

#[test]
fn renamenx_newkey_present_src_still_exists() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "src", "val"]);
    execute(&mut store, &["SET", "dst", "other"]);
    execute(&mut store, &["RENAMENX", "src", "dst"]);
    let result = execute(&mut store, &["EXISTS", "src"]);
    assert_eq!(result, "1");
}

// --- key == newkey (resolved assumption) ---
// When key == newkey and the key exists, newkey already exists → returns "0"

#[test]
fn renamenx_key_equals_newkey_returns_zero() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "same", "value"]);
    let result = execute(&mut store, &["RENAMENX", "same", "same"]);
    assert_eq!(result, "0");
}

#[test]
fn renamenx_key_equals_newkey_value_unchanged() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "same", "value"]);
    execute(&mut store, &["RENAMENX", "same", "same"]);
    let result = execute(&mut store, &["GET", "same"]);
    assert_eq!(result, "value");
}

// --- case insensitive command ---

#[test]
fn renamenx_command_name_case_insensitive() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "k", "v"]);
    let result = execute(&mut store, &["renamenx", "k", "k2"]);
    assert_eq!(result, "1");
}
