
# neam/tools

simply a set of tools for my projects...

Some are creepy, ugly (like `embed` or `execute_pack`) while other are really wonderful (like `embed`, `execute_pack` or `regexp`)

## list of tools included

**NOTE:** on `cr` et `ct`: `ct` is for tools with _only_ a compile time components,
(the only exceptions being `contructor::call` that also perform placement new (but is in `ct` because of its similarity / complementarity with `ct::embed`)
and `cr::tuple` which is in `cr`... until I move it from here / remove it).

- `ct::regexp`: a very easy-to-use compile-time regular expression that can matches string at both compile-time and runtime. ( `neam::ct::regexp<my_reg_exp_str>::match("my string")` )
  It currently handles `(...)`, `...|...`, `...*`, `...?`, `...+`, `$`, `\`, `.`, `...!` (postfix not), `[...]` (match a range), `{n}` `{,m}` `{n,}` `{n,m}` (repeat from n to m time)
- `neam::cr::stream_logger`: a threadsafe, multi-stream logger
- `neam::debug`: (see the file debug/assert.hpp) a simple assertion/debug mechanism with an extensible code-to-error system (currently support openCL and UNIX errors codes)

- `cr::array_wrapper<Type>`: a simple wrapper for arrays (provides a `Type *array` and a `size_t size` properties)
- `ct::embed`: a way to embed values as type
- `ct::constructor::call`: like `embed` but for object instances
- `ct::array`: a compile time array (values are passed as template parameters)
- `demangle`: a simple utility that allow one to demangle simbols at runtime ( _GCC only_ )
- `ct::type_list` (in ct_list.hpp): a simple list of types with some utilities. (also see `ct::type_at_index`)
- `ct::merger`: merge two ct::list, cr::tuple or merger
- `ct::type_at_index` return the type at a given index.
- `NEAM_EXECUTE_PACK`: a way to iterate at compile/runtime time over variadic template's argument packs. No extra cost added. (may be _faster_ than the standard recursion pattern)
- `exception`: an exception class that use `neam::demangle` to insert the type name of a class in the message string.
- `cr::memory_allocator`: works a bit like a subset of `std::deque` but could provides a contiguous buffer if needed.
- `cr::memory_pool`: a really simple (and yet unfinished) memory allocation pool. The current implementation is simple but fast.
- `ownership`: types and instance for using a ownership resource management pattern. (see ownership.hpp for more information)


## author
Timothée Feuillet (_neam_ or _tim42_).
