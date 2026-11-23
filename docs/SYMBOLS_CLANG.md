# Clang-first source symbols

See [Release 3.7, section 2](RELEASE_3_7.md#2-clang-first-symbols--outline) for the authoritative configuration and usage instructions.

The core API is `dodev::symbols::extract(path, contents, Options)`. The `contents` parameter always comes from the active in-memory editor buffer, so no save is required. `Options::mode` supports `Auto`, `ClangOnly` and `RegexOnly`. Results include `Result::parser` (`Clang`, `Regex`, `Regex (fallback)`, or `Clang (unavailable)`) and `Result::diagnostics`.

When adding language support, prefer a proper parser for that language but retain the regex backend as a resilience mechanism. Never execute source content; the Clang subprocess operates in syntax-only mode, directly invoked without a shell, with bounded runtime and AST size.
