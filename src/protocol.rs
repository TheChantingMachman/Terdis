#[derive(Debug, PartialEq)]
pub enum RespValue {
    SimpleString(String),
    Error(String),
    Integer(i64),
    BulkString(String),
    Null,
    Array(Vec<RespValue>),
}

#[derive(Debug, PartialEq)]
pub enum RespError {
    Incomplete,
    Invalid(String),
}

pub fn parse(input: &[u8]) -> Result<(RespValue, usize), RespError> {
    if input.is_empty() {
        return Err(RespError::Incomplete);
    }
    match input[0] {
        b'+' => parse_line(&input[1..]).map(|(s, n)| (RespValue::SimpleString(s), n + 1)),
        b'-' => parse_line(&input[1..]).map(|(s, n)| (RespValue::Error(s), n + 1)),
        b':' => parse_integer_line(&input[1..]).map(|(i, n)| (RespValue::Integer(i), n + 1)),
        b'$' => parse_bulk_string(&input[1..]).map(|(v, n)| (v, n + 1)),
        b'*' => parse_array(&input[1..]).map(|(v, n)| (v, n + 1)),
        _ => Err(RespError::Invalid(format!("unknown type byte: {}", input[0]))),
    }
}

fn find_crlf(input: &[u8]) -> Option<usize> {
    input.windows(2).position(|w| w == b"\r\n")
}

fn parse_line(input: &[u8]) -> Result<(String, usize), RespError> {
    match find_crlf(input) {
        None => Err(RespError::Incomplete),
        Some(pos) => {
            let s = std::str::from_utf8(&input[..pos])
                .map_err(|e| RespError::Invalid(e.to_string()))?
                .to_string();
            Ok((s, pos + 2))
        }
    }
}

fn parse_integer_line(input: &[u8]) -> Result<(i64, usize), RespError> {
    let (s, n) = parse_line(input)?;
    let i = s.parse::<i64>().map_err(|e| RespError::Invalid(e.to_string()))?;
    Ok((i, n))
}

fn parse_bulk_string(input: &[u8]) -> Result<(RespValue, usize), RespError> {
    let (len_str, n) = parse_line(input)?;
    let len: i64 = len_str.parse().map_err(|e: std::num::ParseIntError| RespError::Invalid(e.to_string()))?;
    if len == -1 {
        return Ok((RespValue::Null, n));
    }
    if len < 0 {
        return Err(RespError::Invalid("negative bulk string length".to_string()));
    }
    let len = len as usize;
    let remaining = &input[n..];
    if remaining.len() < len + 2 {
        return Err(RespError::Incomplete);
    }
    let s = std::str::from_utf8(&remaining[..len])
        .map_err(|e| RespError::Invalid(e.to_string()))?
        .to_string();
    if remaining[len] != b'\r' || remaining[len + 1] != b'\n' {
        return Err(RespError::Invalid("missing CRLF after bulk string data".to_string()));
    }
    Ok((RespValue::BulkString(s), n + len + 2))
}

fn parse_array(input: &[u8]) -> Result<(RespValue, usize), RespError> {
    let (count_str, mut consumed) = parse_line(input)?;
    let count: i64 = count_str.parse().map_err(|e: std::num::ParseIntError| RespError::Invalid(e.to_string()))?;
    if count == -1 {
        return Ok((RespValue::Null, consumed));
    }
    if count < 0 {
        return Err(RespError::Invalid("negative array count".to_string()));
    }
    let count = count as usize;
    let mut elements = Vec::with_capacity(count);
    for _ in 0..count {
        let (val, n) = parse(&input[consumed..])?;
        consumed += n;
        elements.push(val);
    }
    Ok((RespValue::Array(elements), consumed))
}

pub fn encode_value(value: &RespValue) -> Vec<u8> {
    match value {
        RespValue::SimpleString(s) => format!("+{}\r\n", s).into_bytes(),
        RespValue::Error(s) => format!("-{}\r\n", s).into_bytes(),
        RespValue::Integer(i) => format!(":{}\r\n", i).into_bytes(),
        RespValue::BulkString(s) => format!("${}\r\n{}\r\n", s.len(), s).into_bytes(),
        RespValue::Null => b"$-1\r\n".to_vec(),
        RespValue::Array(items) => {
            let mut out = format!("*{}\r\n", items.len()).into_bytes();
            for item in items {
                out.extend(encode_value(item));
            }
            out
        }
    }
}

pub fn encode_response(response: &str) -> Vec<u8> {
    if let Some(rest) = response.strip_prefix("(error) ") {
        return encode_value(&RespValue::Error(rest.to_string()));
    }
    if response == "(nil)" {
        return encode_value(&RespValue::Null);
    }
    if let Ok(i) = response.parse::<i64>() {
        return encode_value(&RespValue::Integer(i));
    }
    if response.contains('\n') {
        let parts: Vec<RespValue> = response.split('\n').map(|part| {
            if part == "(nil)" {
                RespValue::Null
            } else {
                RespValue::BulkString(part.to_string())
            }
        }).collect();
        return encode_value(&RespValue::Array(parts));
    }
    encode_value(&RespValue::BulkString(response.to_string()))
}
