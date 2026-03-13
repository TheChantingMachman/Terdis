use std::collections::HashMap;

pub struct Store {
    data: HashMap<String, String>,
}

impl Store {
    pub fn new() -> Self {
        Store {
            data: HashMap::new(),
        }
    }
}

pub fn execute(store: &mut Store, args: &[&str]) -> String {
    if args.is_empty() {
        return "(error) ERR unknown command".to_string();
    }
    match args[0].to_uppercase().as_str() {
        "SET" => {
            if args.len() - 1 != 2 {
                return "(error) ERR wrong number of arguments for 'SET' command".to_string();
            }
            store.data.insert(args[1].to_string(), args[2].to_string());
            "OK".to_string()
        }
        "GET" => {
            if args.len() - 1 != 1 {
                return "(error) ERR wrong number of arguments for 'GET' command".to_string();
            }
            match store.data.get(args[1]) {
                Some(v) => v.clone(),
                None => "(nil)".to_string(),
            }
        }
        "DEL" => {
            if args.len() - 1 < 1 {
                return "(error) ERR wrong number of arguments for 'DEL' command".to_string();
            }
            let count = args[1..].iter().filter(|k| store.data.remove(**k).is_some()).count();
            count.to_string()
        }
        "EXISTS" => {
            if args.len() - 1 < 1 {
                return "(error) ERR wrong number of arguments for 'EXISTS' command".to_string();
            }
            let count = args[1..].iter().filter(|k| store.data.contains_key(**k)).count();
            count.to_string()
        }
        _ => "(error) ERR unknown command".to_string(),
    }
}
