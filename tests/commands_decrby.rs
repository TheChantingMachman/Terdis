use terdis::{execute, store::Store};

#[test]
fn decrby_missing_key_sets_to_zero_then_decrements() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECRBY", "counter", "5"]);
    assert_eq!(result, "-5");
}

#[test]
fn decrby_missing_key_stores_result() {
    let mut store = Store::new();
    execute(&mut store, &["DECRBY", "counter", "3"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "-3");
}

#[test]
fn decrby_existing_integer_decrements_by_amount() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["DECRBY", "counter", "4"]);
    assert_eq!(result, "6");
}

#[test]
fn decrby_updates_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    execute(&mut store, &["DECRBY", "counter", "3"]);
    let result = execute(&mut store, &["GET", "counter"]);
    assert_eq!(result, "7");
}

#[test]
fn decrby_negative_decrement_increments() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "10"]);
    let result = execute(&mut store, &["DECRBY", "counter", "-3"]);
    assert_eq!(result, "13");
}

#[test]
fn decrby_negative_key_and_positive_decrement() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "-10"]);
    let result = execute(&mut store, &["DECRBY", "counter", "4"]);
    assert_eq!(result, "-14");
}

#[test]
fn decrby_zero_decrement_returns_same_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "counter", "42"]);
    let result = execute(&mut store, &["DECRBY", "counter", "0"]);
    assert_eq!(result, "42");
}

#[test]
fn decrby_non_integer_stored_value_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    let result = execute(&mut store, &["DECRBY", "key", "5"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decrby_non_integer_stored_value_does_not_update() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "notanumber"]);
    execute(&mut store, &["DECRBY", "key", "5"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "notanumber");
}

#[test]
fn decrby_non_integer_decrement_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    let result = execute(&mut store, &["DECRBY", "key", "notanumber"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decrby_non_integer_decrement_does_not_update() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    execute(&mut store, &["DECRBY", "key", "notanumber"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "10");
}

#[test]
fn decrby_float_decrement_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "10"]);
    let result = execute(&mut store, &["DECRBY", "key", "1.5"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decrby_underflow_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "-9223372036854775808"]);
    let result = execute(&mut store, &["DECRBY", "key", "1"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decrby_underflow_does_not_update_stored_value() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "-9223372036854775808"]);
    execute(&mut store, &["DECRBY", "key", "1"]);
    let result = execute(&mut store, &["GET", "key"]);
    assert_eq!(result, "-9223372036854775808");
}

#[test]
fn decrby_overflow_via_negative_decrement_returns_error() {
    let mut store = Store::new();
    execute(&mut store, &["SET", "key", "9223372036854775807"]);
    let result = execute(&mut store, &["DECRBY", "key", "-1"]);
    assert_eq!(result, "(error) ERR value is not an integer or out of range");
}

#[test]
fn decrby_zero_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECRBY"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DECRBY' command");
}

#[test]
fn decrby_one_arg_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECRBY", "key"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DECRBY' command");
}

#[test]
fn decrby_three_args_returns_error() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DECRBY", "key", "5", "extra"]);
    assert_eq!(result, "(error) ERR wrong number of arguments for 'DECRBY' command");
}

#[test]
fn decrby_case_insensitive_lowercase() {
    let mut store = Store::new();
    let result = execute(&mut store, &["decrby", "counter", "3"]);
    assert_eq!(result, "-3");
}

#[test]
fn decrby_case_insensitive_mixed() {
    let mut store = Store::new();
    let result = execute(&mut store, &["DecrBy", "counter", "3"]);
    assert_eq!(result, "-3");
}
