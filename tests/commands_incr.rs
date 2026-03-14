use terdis::{execute, store::Store};

#[test]
fn incr_missing_key_sets_to_zero_then_increments() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCR", "counter"]);
    assert_eq!(result, "1");
}

#[test]
fn incr_missing_key_stores_result() {
    let mut store = Store::new();
    execute(&mut store, &["INCR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "1");
}

#[test]
fn incr_existing_integer_increments_by_one() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["INCR", "counter"]);
    assert_eq!(result, "11");
}

#[test]
fn incr_updates_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "5"]);
    execute(&mut store, &["INCR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "6");
}

#[test]
fn incr_multiple_times_accumulates() {
    let mut store = Store::new();
    execute(&mut store, &["INCR", "counter"]);
    execute(&mut store, &["INCR", "counter"]);
    execute(&mut store, &["INCR", "counter"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "3");
}

#[test]
fn incr_negative_integer_increments_toward_zero() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "-5"]);
    let result = execute(&mut store, &["INCR", "counter"]);
    assert_eq!(result, "-4");
}

#[test]
fn incr_non_integer_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    let result = execute(&mut store, &["INCR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incr_non_integer_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    execute(&mut store, &["INCR", "key"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "notanumber");
}

#[test]
fn incr_float_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "3.14"]);
    let result = execute(&mut store, &["INCR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incr_overflow_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "9223372036854775807"]);
    let result = execute(&mut store, &["INCR", "key"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incr_overflow_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "9223372036854775807"]);
    execute(&mut store, &["INCR", "key"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "9223372036854775807");
}

#[test]
fn incr_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCR"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'INCR' command");
}

#[test]
fn incr_two_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCR", "key", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'INCR' command");
}

#[test]
fn incr_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["incr", "counter"]);
    assert_eq!(result, "1");
}

#[test]
fn incr_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["Incr", "counter"]);
    assert_eq!(result, "1");
}
