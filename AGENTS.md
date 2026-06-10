- Never hand-align or hand-format code (structs, function signatures, etc)
- Do not add comments. Ever. Code with comments will be outright rejected; always do a pass over your changes before completion to ensure no comments were added.
- Treat .build/source/sp/AGENTS.md as our canonical style guide for C code

# errors
- If a function can fail, it must return an error
  - Simple errors should be a typed numeric code (`typedef enum { ... } some_err_t`)
  - For errors that have context, use a tagged union where the tag is the error kind
  - Nullability can be used instead if callers don't handle specific errors
- Never use strings as errors; an error may only be converted to a string for logging
- Use the `sp_try()` family of macros to ergonomically propagate errors up the call stack
