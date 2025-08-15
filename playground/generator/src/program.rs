use crate::constants::*;
use crate::parser;
use elf::symbol::Symbol;

pub struct Utilities {}

impl Utilities {
    fn utils(&self) -> String {
        let lines = vec![UTILS];

        lines.join("\n")
    }
}

pub struct Definitions {
    memo: Vec<(String, Symbol)>,
    custom_code: String,
    head_code: String,
}
impl Definitions {
    pub fn new( custom_code : &str) -> Self {
        Definitions { 
            memo: Vec::new(),
            custom_code: custom_code.to_string(),
            head_code: String::new(),
        }
    }

    pub fn add_hook_code(&mut self, code: &str) {
        self.head_code.push_str(code);
        self.head_code.push('\n');
    }

    pub fn add_memo(&mut self, name: &str, symbol: Symbol) {
        self.memo.push((name.to_string(), symbol));
    }

    fn memo_entries(&self) -> String {
        let mut lines = Vec::new();
        lines.push("  // Memo entries".to_string());
        for (name, symbol) in &self.memo {
            lines.push(self.memo_entry(name, symbol));
        }
        lines.join("\n")
    }

    fn memo_entry(&self, name: &str, symbol: &Symbol) -> String {
        let index = self.memo_constant(&name);
        format!(
            "  memo[{}] = (entry){{\n    \"{}\",\n    (void*)0x{:X},\n  }};",
            index, name, symbol.st_value
        )
    }

    fn memo_constant(&self, name: &str) -> String {
        parser::to_constant_case(&parser::parse(name, 0))
    }

    fn memo_indices(&self) -> String {
        let mut lines = Vec::new();
        for (name, _) in &self.memo {
            lines.push(format!(
                "#define {} {}",
                self.memo_constant(&name),
                lines.len()
            ));
        }
        lines.join("\n")
    }

    pub fn main(&self) -> String {
        let memo_entries = self.memo_entries();

        let lines = vec![
            MAIN_OPEN,
            ALLOCATED_NEAR_MEMORY,
            &memo_entries,
            SEPARATOR,
            SEPARATOR,
            CUSTOM_OPEN,
            &self.custom_code,
            CUSTOM_CLOSE,
            SEPARATOR,
            MAIN_CLOSE,
        ];
        lines.join("\n")
    }

    fn memo_definition(&self, length: usize) -> String {
        format!("entry memo[{}];", length)
    }

    pub fn head(&self) -> String {
        let defs = self.memo_definition(self.memo.len());
        let memo_indices = self.memo_indices();

        let lines : Vec<&str> = vec![
            &defs, 
            &memo_indices,
            &self.head_code,
        ];
        lines.join("\n")
    }

    pub fn program(&self) -> String {
        self.head() + "\n" + self.main().as_str()
    }
}

pub struct Program {
    utilities: Utilities,
    definitions: Definitions,
}

impl Program {
    pub fn new(custom_code : &str) -> Self {
        Program {
            utilities: Utilities {},
            definitions: Definitions::new(custom_code),
        }
    }

    pub fn add_wrapper(&mut self, code : &str){
        self.definitions.add_hook_code(code);
    }

    pub fn from_existing() -> Self {
        let generated_code = std::fs::read_to_string("generated_code.c").expect("Unable to read file");
        println!("{}", generated_code);
        // parse out custom code from the file 
        // custom code is between CUSTOM_OPEN and CUSTOM_CLOSE
        let start = generated_code.find(CUSTOM_OPEN).unwrap() + CUSTOM_OPEN.len();
        let end = generated_code.find(CUSTOM_CLOSE).unwrap();
        // make sure to trim out new lines
        let custom_code = &generated_code[start+1..end-1];
        Program::new(custom_code)
    }

    pub fn add_memo(&mut self, name: &str, symbol: Symbol) {
        self.definitions.add_memo(name, symbol);
    }

    pub fn add_hook() {
    }
    pub fn generate(&self) -> String {
        let mut result = String::new();
        result += &self.utilities.utils();
        result += &self.definitions.program();
        result
    }
}
