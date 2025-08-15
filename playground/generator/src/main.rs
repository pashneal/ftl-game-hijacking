mod constants;
mod parser;
mod signatures;
mod program;
mod elf_parse;

use program::Program;




fn main() {
    // Use the hash table to find a given symbol in it.
    let map = elf_parse::create_symbol_mapping("./FTL.amd64");

    let mut program = Program::new("");
    if std::path::Path::new("generated_code.c").exists() {
        program = Program::from_existing();
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
