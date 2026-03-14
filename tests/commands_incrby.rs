use terdis::{execute, store::Store};

#[test]
fn incrby_missing_key_sets_to_zero_then_increments() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCRBY", "counter", "5"]);
    assert_eq!(result, "5");
}

#[test]
fn incrby_missing_key_stores_result() {
    let mut store = Store::new();
    execute(&mut store, &["INCRBY", "counter", "7"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "7");
}

#[test]
fn incrby_existing_integer_increments_by_amount() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["INCRBY", "counter", "5"]);
    assert_eq!(result, "15");
}

#[test]
fn incrby_updates_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    execute(&mut store, &["INCRBY", "counter", "3"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "13");
}

#[test]
fn incrby_negative_increment_decrements() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["INCRBY", "counter", "-3"]);
    assert_eq!(result, "7");
}

#[test]
fn incrby_negative_key_and_positive_increment() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "-10"]);
    let result = execute(&mut store, &["INCRBY", "counter", "4"]);
    assert_eq!(result, "-6");
}

#[test]
fn incrby_zero_increment_returns_same_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "42"]);
    let result = execute(&mut store, &["INCRBY", "counter", "0"]);
    assert_eq!(result, "42");
}

#[test]
fn incrby_non_integer_stored_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    let result = execute(&mut store, &["INCRBY", "key", "5"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incrby_non_integer_stored_value_does_not_update() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    execute(&mut store, &["INCRBY", "key", "5"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "notanumber");
}

#[test]
fn incrby_non_integer_increment_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    let result = execute(&mut store, &["INCRBY", "key", "notanumber"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incrby_non_integer_increment_does_not_update() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    execute(&mut store, &["INCRBY", "key", "notanumber"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "10");
}

#[test]
fn incrby_float_increment_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    let result = execute(&mut store, &["INCRBY", "key", "1.5"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incrby_overflow_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "9223372036854775807"]);
    let result = execute(&mut store, &["INCRBY", "key", "1"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incrby_overflow_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "9223372036854775807"]);
    execute(&mut store, &["INCRBY", "key", "1"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "9223372036854775807");
}

#[test]
fn incrby_underflow_via_negative_increment_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "-9223372036854775808"]);
    let result = execute(&mut store, &["INCRBY", "key", "-1"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn incrby_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCRBY"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'INCRBY' command");
}

#[test]
fn incrby_one_arg_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCRBY", "key"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'INCRBY' command");
}

#[test]
fn incrby_three_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["INCRBY", "key", "5", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'INCRBY' command");
}

#[test]
fn incrby_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["incrby", "counter", "3"]);
    assert_eq!(result, "3");
}

#[test]
fn incrby_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["IncrBy", "counter", "3"]);
    assert_eq!(result, "3");
}
