use crate::store::Store;

pub fn cmd_set(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 2 {
        return "(error) ERR wrong number of arguments for 'SET' command".to_string();
    }
    store.set(args[0].to_string(), args[1].to_string());
    "OK".to_string()
}

pub fn cmd_get(store: &Store, args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'GET' command".to_string();
    }
    match store.get(args[0]) {
        Some(v) => v.clone(),
        None => "(nil)".to_string(),
    }
}

pub fn cmd_del(store: &mut Store, args: &[&str]) -> String {
    if args.is_empty() {
        return "(error) ERR wrong number of arguments for 'DEL' command".to_string();
    }
    let mut count = 0usize;
    for key in args {
        if store.del(key) {
            count += 1;
        }
    }
    count.to_string()
}

pub fn cmd_exists(store: &Store, args: &[&str]) -> String {
    if args.is_empty() {
        return "(error) ERR wrong number of arguments for 'EXISTS' command".to_string();
    }
    let count = args.iter().filter(|k| store.exists(k)).count();
    count.to_string()
}

pub fn cmd_ping(args: &[&str]) -> String {
    match args.len() {
        0 => "PONG".to_string(),
        1 => args[0].to_string(),
        _ => "(error) ERR wrong number of arguments for 'PING' command".to_string(),
    }
}

pub fn cmd_echo(args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'ECHO' command".to_string();
    }
    args[0].to_string()
}

pub fn cmd_append(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 2 {
        return "(error) ERR wrong number of arguments for 'APPEND' command".to_string();
    }
    let key = args[0];
    let value = args[1];
    let new_val = match store.get(key) {
        Some(existing) => format!("{}{}", existing, value),
        None => value.to_string(),
    };
    let len = new_val.len();
    store.set(key.to_string(), new_val);
    len.to_string()
}

pub fn cmd_strlen(store: &Store, args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'STRLEN' command".to_string();
    }
    match store.get(args[0]) {
        Some(v) => v.len().to_string(),
        None => "0".to_string(),
    }
}

pub fn cmd_mset(store: &mut Store, args: &[&str]) -> String {
    if args.is_empty() || args.len() % 2 != 0 {
        return "(error) ERR wrong number of arguments for 'MSET' command".to_string();
    }
    for pair in args.chunks(2) {
        store.set(pair[0].to_string(), pair[1].to_string());
    }
    "OK".to_string()
}

pub fn cmd_type(store: &Store, args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'TYPE' command".to_string();
    }
    if store.exists(args[0]) {
        "string".to_string()
    } else {
        "none".to_string()
    }
}

pub fn cmd_dbsize(store: &Store, args: &[&str]) -> String {
    if !args.is_empty() {
        return "(error) ERR wrong number of arguments for 'DBSIZE' command".to_string();
    }
    store.len().to_string()
}

pub fn cmd_mget(store: &Store, args: &[&str]) -> String {
    if args.is_empty() {
        return "(error) ERR wrong number of arguments for 'MGET' command".to_string();
    }
    let values: Vec<String> = args
        .iter()
        .map(|k| match store.get(k) {
            Some(v) => v.clone(),
            None => "(nil)".to_string(),
        })
        .collect();
    values.join("\n")
}

const ERR_NOT_INT: &str = "(error) ERR value is not an integer or out of range";

fn get_int_value(store: &Store, key: &str) -> Result<i64, String> {
    match store.get(key) {
        None => Ok(0),
        Some(v) => v.parse::<i64>().map_err(|_| ERR_NOT_INT.to_string()),
    }
}

pub fn cmd_incr(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'INCR' command".to_string();
    }
    let key = args[0];
    match get_int_value(store, key) {
        Err(e) => e,
        Ok(n) => match n.checked_add(1) {
            None => ERR_NOT_INT.to_string(),
            Some(result) => {
                store.set(key.to_string(), result.to_string());
                result.to_string()
            }
        },
    }
}

pub fn cmd_decr(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 1 {
        return "(error) ERR wrong number of arguments for 'DECR' command".to_string();
    }
    let key = args[0];
    match get_int_value(store, key) {
        Err(e) => e,
        Ok(n) => match n.checked_sub(1) {
            None => ERR_NOT_INT.to_string(),
            Some(result) => {
                store.set(key.to_string(), result.to_string());
                result.to_string()
            }
        },
    }
}

pub fn cmd_incrby(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 2 {
        return "(error) ERR wrong number of arguments for 'INCRBY' command".to_string();
    }
    let key = args[0];
    let delta: i64 = match args[1].parse() {
        Ok(v) => v,
        Err(_) => return ERR_NOT_INT.to_string(),
    };
    match get_int_value(store, key) {
        Err(e) => e,
        Ok(n) => match n.checked_add(delta) {
            None => ERR_NOT_INT.to_string(),
            Some(result) => {
                store.set(key.to_string(), result.to_string());
                result.to_string()
            }
        },
    }
}

pub fn cmd_decrby(store: &mut Store, args: &[&str]) -> String {
    if args.len() != 2 {
        return "(error) ERR wrong number of arguments for 'DECRBY' command".to_string();
    }
    let key = args[0];
    let delta: i64 = match args[1].parse() {
        Ok(v) => v,
        Err(_) => return ERR_NOT_INT.to_string(),
    };
    match get_int_value(store, key) {
        Err(e) => e,
        Ok(n) => match n.checked_sub(delta) {
            None => ERR_NOT_INT.to_string(),
            Some(result) => {
                store.set(key.to_string(), result.to_string());
                result.to_string()
            }
        },
    }
}
