pub fn parse(mangled: &str, num: usize) -> String {
    if mangled.len() == 0 {
        return "".to_string();
    }

    if mangled.chars().next().unwrap().is_digit(10) {
        return number(&mangled, num);
    }

    if num > 0 {
        return take_chars(mangled, num);
    }

    return skip_char(mangled, num);
}

fn number(s: &str, num: usize) -> String {
    let snum = s[0..1].parse::<usize>().unwrap();
    let num = num * 10 + snum;

    parse(&s[1..], num)
}

fn take_chars(s: &str, num: usize) -> String {
    let value = s[0..num].to_string();
    let rest = &s[num..];

    return value + &parse(rest, 0);
}

fn skip_char(s: &str, _num: usize) -> String {
    return parse(&s[1..], 0);
}

pub fn to_constant_case(s: &str) -> String {
    let mut result = String::new();
    for (i, c) in s.chars().enumerate() {
        if c.is_uppercase() && i != 0 {
            result.push('_');
        }
        result.push(c.to_ascii_uppercase());
    }
    return result;
}
