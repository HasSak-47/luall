use mlua::ffi::lua_State;
use tree_sitter::{InputEdit, Language, Parser, Point};
use tree_sitter_highlight::{HighlightConfiguration, Highlighter};
use tree_sitter_lyra::{HIGHLIGHTS_QUERY, LANGUAGE, LOCALS_QUERY};

#[allow(nonstandard_style)]
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Event {
    EVENT_KEY_INPUT = 0,
    EVENT_ENTER = 1,
    EVENT_EXIT = 2,
}

pub type Actor = Option<unsafe extern "C" fn(l: *mut lua_State) -> i32>;

unsafe extern "C" {
    pub fn add_hook(event: Event, actor: Actor);
}

struct TSHanlder {
    lang: Language,
    parser: Parser,
    hl_config: HighlightConfiguration,
    highlighter: Highlighter,
}

impl TSHanlder {
    pub fn new() -> anyhow::Result<Self> {
        let lang: Language = LANGUAGE.into();

        let mut parser = Parser::new();
        parser.set_language(&lang)?;

        let highlighter = Highlighter::new();
        let hl_config =
            HighlightConfiguration::new(lang.clone(), "lyra", HIGHLIGHTS_QUERY, "", LOCALS_QUERY)?;

        return Ok(TSHanlder {
            parser,
            hl_config,
            lang,
            highlighter,
        });
    }
}

impl mlua::UserData for TSHanlder {
    fn add_methods<M: mlua::UserDataMethods<Self>>(methods: &mut M) {
        methods.add_function("new", |_, ()| {
            return Ok(TSHanlder::new()?);
        });
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn plugin_setup(l: *mut lua_State) -> i32 {
    if l.is_null() {
        return 0;
    }
    let lua = unsafe { mlua::Lua::get_or_init_from_ptr(l) };

    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn plugin_destruct(l: *mut lua_State) -> i32 {
    if l.is_null() {
        return 0;
    }

    let lua = unsafe { mlua::Lua::get_or_init_from_ptr(l) };
    0
}
