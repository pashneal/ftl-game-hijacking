use elf::ElfBytes;
use elf::endian::AnyEndian;
use elf::symbol::Symbol;
use std::collections::HashMap;

// Map names to symbols from the parsed ELF file
pub fn create_symbol_mapping(filename: &str) -> HashMap<String, Symbol> {
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
