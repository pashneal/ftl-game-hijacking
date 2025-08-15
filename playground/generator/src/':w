use crate::signatures::Type;

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

pub struct FuncWrapperBuilder {
    args : Option<Vec<Type>>,
    return_type: Option<Type>,
    func_name: Option<String>,
    custom_code: Option<String>,
    target_memo: Option<String>,
}

impl FuncWrapperBuilder {
    pub fn new() -> Self {
        FuncWrapperBuilder {
            args: None,
            return_type: None,
            func_name: None,
            custom_code: None,
            target_memo: None,
            
        }
    }

    pub fn args(mut self, args: Vec<Type>) -> Self {
        self.args = Some(args);
        self
    }

    pub fn return_type(mut self, return_type: Type) -> Self {
        self.return_type = Some(return_type);
        self
    }

    pub fn name(mut self, name: &str) -> Self {
        self.func_name = Some(name.to_string());
        self
    }
    
    pub fn custom_code(mut self, custom_code: &str) -> Self {
        self.custom_code = Some(custom_code.to_string());
        self
    }

    pub fn target_memo(mut self, target_memo: &str) -> Self {
        self.target_memo = Some(target_memo.to_string());
        self
    }

    pub fn build(self) -> Result<FuncWrapper, String> {
        if let (Some(args), Some(return_type), Some(func_name), Some(target_memo)) = (self.args, self.return_type, self.func_name, self.target_memo) {
            Ok(FuncWrapper{
                args,
                return_type,
                func_name,
                target_memo,
                custom_code: self.custom_code,
            })
        } else {
            Err("Function is not fully defined".to_string())
        }
    }
}

pub struct FuncWrapper  {
    args: Vec<Type>,
    return_type: Type,
    func_name: String,
    custom_code: Option<String>,
    target_memo: String,
}

impl FuncWrapper {

    fn typedef(&self) -> String {
        func_typedef(self.args.clone(), self.return_type.clone(), &self.func_name)
    }

    fn cast(&self) -> String {
        func_cast(&self.target_memo, &self.func_name)
    }

    pub fn code(&self) -> String {
        let return_type = self.return_type.clone();
        let custom_code = match &self.custom_code {
            Some(code) => code.clone(),
            None => "".to_string(),
        };
        let typedef = self.typedef();
        let cast = self.cast();
        let return_type_str : &str = return_type.into();
        format!(
r#"
{} {}_wrapper() {{ 
  {}
  {}
{}
}}
"#,     return_type_str, self.func_name, typedef, cast, custom_code)
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
