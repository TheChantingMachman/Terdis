pub mod store;
pub mod commands;

use store::Store;
use commands::{cmd_set, cmd_get, cmd_del, cmd_exists, cmd_ping, cmd_echo, cmd_append, cmd_strlen, cmd_mset, cmd_mget};

pub fn execute(store: &mut Store, args: &[&str]) -> String {
    if args.is_empty() {
        return "(error) ERR no command provided".to_string();
    }
    let cmd = args[0];
    let rest = &args[1..];
    match cmd.to_uppercase().as_str() {
        "SET" => cmd_set(store, rest),
        "GET" => cmd_get(store, rest),
        "DEL" => cmd_del(store, rest),
        "EXISTS" => cmd_exists(store, rest),
        "PING" => cmd_ping(rest),
        "ECHO" => cmd_echo(rest),
        "APPEND" => cmd_append(store, rest),
        "STRLEN" => cmd_strlen(store, rest),
        "MSET" => cmd_mset(store, rest),
        "MGET" => cmd_mget(store, rest),
        _ => format!("(error) ERR unknown command '{}'", cmd),
    }
}
