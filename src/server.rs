use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream, SocketAddr};
use std::sync::{Arc, Mutex};
use std::thread;

use crate::store::Store;
use crate::protocol::{parse, encode_response, encode_value, RespValue, RespError};

pub fn start_server(addr: &str, store: Arc<Mutex<Store>>) -> std::io::Result<SocketAddr> {
    let listener = TcpListener::bind(addr)?;
    let local_addr = listener.local_addr()?;
    thread::spawn(move || {
        for stream in listener.incoming() {
            if let Ok(stream) = stream {
                let store = Arc::clone(&store);
                thread::spawn(move || handle_client(stream, store));
            }
        }
    });
    Ok(local_addr)
}

pub fn handle_client(mut stream: TcpStream, store: Arc<Mutex<Store>>) {
    let mut buf: Vec<u8> = Vec::new();
    let mut tmp = [0u8; 4096];
    loop {
        match stream.read(&mut tmp) {
            Ok(0) => break,
            Ok(n) => {
                buf.extend_from_slice(&tmp[..n]);
                loop {
                    match parse(&buf) {
                        Ok((value, consumed)) => {
                            buf.drain(..consumed);
                            let response = dispatch(&value, &store);
                            let encoded = encode_response(&response);
                            if stream.write_all(&encoded).is_err() {
                                return;
                            }
                        }
                        Err(RespError::Incomplete) => break,
                        Err(RespError::Invalid(msg)) => {
                            let err = encode_value(&RespValue::Error(format!("ERR {}", msg)));
                            let _ = stream.write_all(&err);
                            buf.clear();
                            break;
                        }
                    }
                }
            }
            Err(_) => break,
        }
    }
}

fn dispatch(value: &RespValue, store: &Arc<Mutex<Store>>) -> String {
    let items = match value {
        RespValue::Array(items) => items,
        _ => return "(error) ERR expected array command".to_string(),
    };
    let strings: Vec<String> = items.iter().filter_map(|item| match item {
        RespValue::BulkString(s) | RespValue::SimpleString(s) => Some(s.clone()),
        _ => None,
    }).collect();
    let args: Vec<&str> = strings.iter().map(|s| s.as_str()).collect();
    let mut guard = store.lock().unwrap();
    crate::execute(&mut guard, &args)
}
