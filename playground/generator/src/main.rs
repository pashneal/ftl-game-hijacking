mod constants;
mod parser;
use constants::*;

use elf::ElfBytes;
use elf::endian::AnyEndian;
use elf::symbol::Symbol;
use std::collections::HashMap;

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
}
impl Definitions {
    pub fn new( custom_code : &str) -> Self {
        Definitions { 
            memo: Vec::new(),
            custom_code: custom_code.to_string(),
        }
    }

    pub fn add_memo(&mut self, name: &str, symbol: Symbol) {
        self.memo.push((name.to_string(), symbol));
    }

    fn memo_entries(&self) -> String {
        let mut lines = Vec::new();
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
            CUSTOM_OPEN,
            &self.custom_code,
            CUSTOM_CLOSE,
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

    pub fn add_memo(&mut self, name: &str, symbol: Symbol) {
        self.definitions.add_memo(name, symbol);
    }

    pub fn generate(&self) -> String {
        let mut result = String::new();
        result += &self.utilities.utils();
        result += &self.definitions.program();
        result
    }
}

// Map names to symbols from the parsed ELF file
fn create_symbol_mapping(filename: &str) -> HashMap<String, Symbol> {
    let path = std::path::PathBuf::from(filename);
    let file_data = std::fs::read(path).expect("Could not read file.");
    let slice = file_data.as_slice();
    let file = ElfBytes::<AnyEndian>::minimal_parse(slice).expect("Open test1");

    // Find lazy-parsing types for the common ELF sections (we want .dynsym, .dynstr, .hash)
    let common = file.find_common_data().expect("shdrs should parse");
    let (dynsyms, strtab) = (common.dynsyms.unwrap(), common.dynsyms_strs.unwrap());

    let mut map: HashMap<String, Symbol> = HashMap::new();
    for sym in dynsyms.iter() {
        match strtab.get(sym.st_name as usize) {
            Ok(name) => {
                map.insert(name.to_string(), sym.clone());
            }
            Err(_) => {}
        }
    }

    let (symtab, strtab) = (common.symtab.unwrap(), common.symtab_strs.unwrap());
    for sym in symtab.iter() {
        match strtab.get(sym.st_name as usize) {
            Ok(name) => {
                map.insert(name.to_string(), sym.clone());
            }
            Err(_) => {}
        }
    }

    map
}

fn main() {
    // Use the hash table to find a given symbol in it.
    let map = create_symbol_mapping("./FTL.amd64");

    let mut program = Program::new("");


    // check if we already generated code (saved as generated_code.c)
    if std::path::Path::new("generated_code.c").exists() {
        let generated_code = std::fs::read_to_string("generated_code.c").expect("Unable to read file");
        println!("{}", generated_code);
        // parse out custom code from the file 
        // custom code is between CUSTOM_OPEN and CUSTOM_CLOSE
        let start = generated_code.find(CUSTOM_OPEN).unwrap() + CUSTOM_OPEN.len();
        let end = generated_code.find(CUSTOM_CLOSE).unwrap();
        // make sure to trim out new lines
        let custom_code = &generated_code[start+1..end-1];
        program = Program::new(custom_code);
    }


    let name = "_ZN13WeaponControl7KeyDownEi";
    let symbol = map.get(name).unwrap();
    program.add_memo(name, symbol.clone());

    let name = "_Z7ftl_logPKcz";
    let symbol = map.get(name).unwrap();
    program.add_memo(name, symbol.clone());

    let generated_code = program.generate();
    println!("{}", generated_code);

    // save generated code to a file
    std::fs::write("generated_code.c", generated_code).expect("Unable to write file");


    // Goals:
    // 1. generate a hook  that can call the original function using the memo
    // 2. generate a hook that overwrites the original function 
    // 3. generate a hook that saves the overwritten bytes and restores function call
    //    after executing custom code (can use elf parsing maybe to 
    //    determine overwritten bytes)
    //
    // 4. generate a socket and something that feeds into the socket
}
