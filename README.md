# Ciberian Programming Language

Ciberian strongly statically typed interpreted language written in C

```rust
fn main() : void {
    std.print "Hello" " World!\n";
}
```
# 1000-7=ghoul
```rust
fn main () : void {
    i32 count = 1000;
    while(count>0){
        std.print count " - 7 = ";
        count = count - 7;
        std.print count "\n";
    }
}
```

# compiling

```console
$ make # or cc src/main.c -o ciberian
```

# running

```console
$ ./ciberian test.cbr # possible --version option (temporary removed)
```

# TODO

Main Aims
 - [x] Variables
 - [x] Variable Scoping
 - [x] If Else
 - [x] While loops
 - [x] Functions
 - [x] Proper math evaluation
 - [ ] IOlib
 - [ ] Strict Typecheking

General
 - [ ] String literals
 - [ ] `#import` directive
 - [ ] For loops
 - [ ] Else If
 - [ ] Syscalls
 - [ ] Allow `_` in function & variable names
 - [ ] Remove curses from error messages

Data Types
 - [x] `i8 i32 i64`
 - [ ] `u8 u32 u64`
 - [x] Strings (like Arrays or Class-like thingy)
 - [x] Arrays
 - [ ] Unsigned types
 - [ ] Pointer modificator
 - [ ] User provided Structs

