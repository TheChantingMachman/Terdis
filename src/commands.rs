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
