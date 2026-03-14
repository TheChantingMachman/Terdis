use terdis::{execute, store::Store};

#[test]
fn decr_missing_key_sets_to_zero_then_decrements() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECR", "counter"]);
    assert_eq!(result, "-1");
}

#[test]
fn decr_missing_key_stores_result() {
    let mut store = Store::new();
    execute(&mut store, &["DECR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "-1");
}

#[test]
fn decr_existing_integer_decrements_by_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["DECR", "counter"]);
    assert_eq!(result, "9");
}

#[test]
fn decr_updates_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "5"]);
    execute(&mut store, &["DECR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "4");
}

#[test]
fn decr_multiple_times_accumulates() {
    let mut store = Store::new();
    execute(&mut store, &["DECR", "counter"]);
    execute(&mut store, &["DECR", "counter"]);
    execute(&mut store, &["DECR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "-3");
}

#[test]
fn decr_negative_integer_decrements_further() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "-5"]);
    let result = execute(&mut store, &["DECR", "counter"]);
    assert_eq!(result, "-6");
}

#[test]
fn decr_non_integer_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    let result = execute(&mut store, &["DECR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decr_non_integer_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    execute(&mut store, &["DECR", "key"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "notanumber");
}

#[test]
fn decr_float_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "3.14"]);
    let result = execute(&mut store, &["DECR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decr_underflow_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "-9223372036854775808"]);
    let result = execute(&mut store, &["DECR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decr_underflow_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "-9223372036854775808"]);
    execute(&mut store, &["DECR", "key"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "-9223372036854775808");
}

#[test]
fn decr_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECR"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DECR' command");
}

#[test]
fn decr_two_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECR", "key", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DECR' command");
}

#[test]
fn decr_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["decr", "counter"]);
    assert_eq!(result, "-1");
}

#[test]
fn decr_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Decr", "counter"]);
    assert_eq!(result, "-1");
}
