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
            "\tmemo[{}] = (entry){{\"{}\", (void*)0x{:X}}};",
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
        let lines = vec![
            MAIN_OPEN,
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

    fn memo_constants(&self) -> Vec<String> {
        vec![]
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
    let name = "_ZN13WeaponControl7KeyDownEi";
    let map = create_symbol_mapping("./FTL.amd64");

    let mut program = Program::new("    puts(\"hello world\");");
    let symbol = map.get(name).unwrap();
    program.add_memo(name, symbol.clone());
    let generated_code = program.generate();
    println!("{}", generated_code);

    // save generated code to a file
    std::fs::write("generated_code.c", generated_code).expect("Unable to write file");


    //let (sym_idx, sym) = hash_table.find(name, &dynsyms, &strtab)
    //.expect("hash table and symbols should parse").unwrap();
    //println!("Found symbol: {} at index {}", strtab.get(sym_idx).unwrap(), sym_idx);
    //let value = sym.st_value;
    //println!("Symbol value: {:#x}", value);
}
