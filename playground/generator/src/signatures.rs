#[derive(Clone, Debug)]
pub enum Type{
    Bool, 
    Int, 
    CharPointer,
    ConstCharPointer,
    Variadic,
    Void,
}

impl Into<&'static str> for Type {
    fn into(self) -> &'static str {
        match self {
            Type::Int => "int",
            Type::CharPointer => "char*",
            Type::ConstCharPointer => "const char*",
            Type::Variadic => "...",
            Type::Void => "void",
            Type::Bool => "bool",
        }
    
    }
}

pub fn func_typedef(args: Vec<Type>, return_type: Type, name: &str) -> String {
    let mut final_string = "typedef ".to_string();
    final_string.push_str(return_type.into());
    final_string.push_str(" (*");
    final_string.push_str(name);
    final_string.push_str("_t");
    final_string.push_str(")(");
    for (index, arg) in args.into_iter().enumerate() {
        if index > 0 {
            final_string.push_str(", ");
        }
        final_string.push_str(arg.into());
    }
    final_string.push_str(");");

    final_string
}

pub fn func_cast(memo_name: &str, func_name: &str) -> String {
    let func_type = format!("{}{}", func_name, "_t");
    let mut final_string = func_type.clone() + " ";
    final_string.push_str(func_name);
    final_string.push_str(" = (");
    final_string.push_str(&func_type);
    final_string.push_str(")memo[");
    final_string.push_str(memo_name);
    final_string.push_str("].addr;");

    final_string
}

pub struct Func  {
    args: Vec<Type>,
    return_type: Type,
    name: String,
}

impl Func {
    pub fn new(args: Vec<Type>, return_type: Type, name: &str) -> Self {
        Func {
            args,
            return_type,
            name: name.to_string(),
        }
    }

    fn typedef(&self) -> String {
        func_typedef(self.args.clone(), self.return_type.clone(), &self.name)
    }

    fn cast(&self, memo_name: &str) -> String {
        func_cast(memo_name, &self.name)
    }

    pub fn wrapper(&self, memo_name: &str, return_type: Type) -> String {
        let typedef = self.typedef();
        let cast = self.cast(memo_name);
        let return_type_str : &str = return_type.into();
        format!(
r#"
{} {}_wrapper() {{ 
  {}
  {}
}}
"#,     return_type_str, self.name, typedef, cast)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_func_wrapper_typedef() {
        let args = vec![Type::Int, Type::CharPointer];
        let return_type = Type::Void;
        let name = "my_function";
        let result = func_typedef(args, return_type, name);
        assert_eq!(result, "typedef void (*my_function_t)(int, char*);");
    }

    #[test]
    fn test_func_wrapper_cast() {
        let memo_name = "MY_FUNCTION_MEMO";
        let func_name = "my_function";
        let result = func_cast(memo_name, func_name);
        assert_eq!(result, "my_function_t my_function = (my_function_t)memo[MY_FUNCTION_MEMO].addr;");
    }
}
