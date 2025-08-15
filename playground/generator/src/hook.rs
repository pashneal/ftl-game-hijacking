fn address_string(address : u64)  -> String {
    let bytes = address.to_le_bytes();
    let hex_parts: Vec<String> = bytes.iter().map(|b| format!("0x{:02X}", b)).collect();
    hex_parts.join(", ")
}

fn relay_hook(trampoline: u64, relay_addr: u64) -> String {
    // Always assume that trampoline is 4 bytes long and after the relay address
    let offset = trampoline - (relay_addr + 5);
    let relay_bytes = offset.to_le_bytes();
    format!(
r#"
  char buffer_{:08X}[5] = {{ 0xE9, 0x{:02X}, 0x{:02X}, 0x{:02x}, 0x{:02x} }};
  overwrite_addr({:08X}, buffer_{:08X}, 5);
"#, 
        relay_addr,
        relay_bytes[0],
        relay_bytes[1],
        relay_bytes[2],
        relay_bytes[3],
        trampoline,
        relay_addr
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_address_string() {
        assert_eq!(address_string(0xDEADBEEF), "0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00");
    }

}

