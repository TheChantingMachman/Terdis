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

    pub fn set(&mut self, key: String, value: String) {
        self.data.insert(key, value);
    }

    pub fn get(&self, key: &str) -> Option<&String> {
        self.data.get(key)
    }

    pub fn del(&mut self, key: &str) -> bool {
        self.data.remove(key).is_some()
    }

    pub fn exists(&self, key: &str) -> bool {
        self.data.contains_key(key)
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn keys(&self) -> Vec<&String> {
        self.data.keys().collect()
    }

    pub fn clear(&mut self) {
        self.data.clear();
    }

    pub fn rename(&mut self, key: &str, newkey: &str) -> bool {
        match self.data.remove(key) {
            None => false,
            Some(val) => {
                self.data.insert(newkey.to_string(), val);
                true
            }
        }
    }
}

impl Default for Store {
    fn default() -> Self {
        Self::new()
    }
}
