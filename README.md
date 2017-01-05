# TCTCTC: The C Tiger Compiler That Compiles

## Introduction

 Follow the instruction of *Modern Compiler implementation in C*, finish the the program part of chapter 12 (as a course lab). Unlike many tiger compilers you can found on github, TCTCTC(**hereafter referred as TCTC**) implements **back-end** and **compiles**.

### This project includes:

- Lexical Analysis (ch 2)
- Parsing (ch 3)
- Abstract Syntax (ch 4)
- Type Checking (ch 5)
- Support for *x86 frame* (ch 6)
- Escape Analysis (ch 6)
- Into a lower IR (ch 7)
- Canoical trees and make basic blocks. (Code provided by the author) (ch 8)
- Instruction selection in *IA32*. (ch 9)
- Flowgraph and Liveness (ch 10)
- Coloring, **including spilling and coalescing**. (ch 11)

### This work can be improved by:

- Make the compiler support macOS & clang linker (currently TCTC can compile a few programs like *queens.tig* on macOS...orz) or other platforms and frames.

- Improve IR, use some fancier translation(like CJUMP in while) described in chapter 7.

- TCTC assumes a fixed length for frame, adjust to scan the frame and generate correctly frame size.

- Better heuristic function in choosing spilled nodes.

- All the advanced topics(CG, OOP, FP, etc.)...

- Write scipts for compile and link into binary.

### Maybe there are bugs in:
- Spilling. When we enable callee-save registers to be moved into temp first and spill them later if necessary, there's infinite spilling for some testcase. Modify x86fram.c to see it...

## Usage

TCTC should compiles in linux. (at least all the testcases in linux)

1. Compile TCTC. You must have `lex` and `bison`(or `yacc`) installed.

 ```
 make
 ```

2. Use TCTC to compile a tiger program. There are some examples in `testcase/`

 ```
 ./a.out <program>
 ```

 Assembly <program>.s will be generated.

3. Link

 For gcc,

 ```
 gcc -Wl,--wrap,getchar -m32 <assembly> runtime.c -o <binary>
 ```

 For clang: 

 *(Can't find clang equivalence for `--wrap`, so it doesn't work when tiger source calls `getchar()`. If a workaround is found, please issue.)*


4. Run

 ```
 ./<binary>
 ```


