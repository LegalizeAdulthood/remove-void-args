# `remove-void-args`

In this kata, we'll walk through the steps of implementing a refactoring
tool for C++ written _in_ C++ using the libraries provided with clang.
Our example refactoring tool will replace `(void)` argument list with `()`.
We'll apply this transformation to a function, method, or typedef.
Our refactoring tool will transform code like this:

```C++
int foo(void) {
    return 0;
}

void bar(void) {
}

class gronk {
public:
    void foo();
    void bar(void);
    gronk(void);
};

void gronk::foo() {
}

void gronk::bar(void) {
}

gronk::gronk(void) {
}
```

into this:

```C++
int foo() {
    return 0;
}

void bar() {
}

class gronk {
public:
    void foo();
    void bar();
    gronk();
};

void gronk::foo() {
}

void gronk::bar() {
}

gronk::gronk() {
}
```

In this kata, we will enhance our solution incrementally to handle all
the places where `(void)` can appear in a function or method signature
in C++:

* `typedef` statements
* forward declaration of functions
* declarations and definitions of function pointers
* declarations and definitions of pointers to methods

## Setting Up

Before we get started, we'll download the source code for `clang`
and build the distribution so we know we've got everything set up
properly.

### Downloading clang source

We'll base our tool on the clang 3.4 code base, which comes as several
source packages:

* [LLVM 3.4](http://llvm.org/releases/3.4/llvm-3.4.src.tar.gz)
* [clang 3.4](http://llvm.org/releases/3.4/clang-3.4.src.tar.gz)
* [clang Tools Extra 3.4](http://llvm.org/releases/3.4/clang-tools-extra-3.4.src.tar.gz)

Windows users will need [CMake](http://www.cmake.org/cmake/resources/software.html)
to prepare the source code for building.

LLVM is the underlying base technology for the intermediate represenation
of code compiled by clang.  Clang is the ISO C++11 compliant compiler built
on LLVM.  Clang Tools Extra provides some additional command-line tools
that are built using libraries provided with clang.

Our code will be based off an existing clang extra tool called
`remove-cstr-calls` that removes redundant calls of `std::string::c_str()`.

Now we need to integrate these three source packages together and prepare
them for building:

1. Unpack LLVM 3.4, you should have a directory named `llvm-3.4`
2. Unpack clang 3.4, you should have a directory named `clang-3.4`
3. Unpack clang Tools Extra 3.4, you should have a directory named `clang-tools-extra-3.4`
4. Rename 'llvm-3.4' to 'llvm'
5. Move the `clang-3.4` directory to `llvm/tools/clang`
6. Move the `clang-tools-extra-3.4` directory to `llvm/tools/clang/tools/extra`

### Building clang

The clang compiler code base is designed to be bootstrapped using the
native compiler to the system.  Follow the instructions on clang's
[Getting Started page](http://clang.llvm.org/get_started.html) for
your operating system with one difference -- since we have downloaded`
source packages, we don't need to checkout source code from subversion.

1. Create a directory for build outputs called `build`, as a sibling of `llvm`
2. `cd build`
3. For **Linux/Macintosh** users: `../llvm/configure; make`
4. For **Windows** users: `cmake -G "Visual Studio 11" ..\llvm` and then build
`LLVM.sln` from within Visual Studio 2012 or later.
5. Now relax and have lunch or dinner.  Seriously, we are about to build
a large codebase and even with a fast multicore machine and builds running
in parallel it will take quite a bit of time to build everything.  While
the code is building, you can continue reading to familiarize yourself
with clang's `libtooling` that provides the support for refactoring tools.

## Examining `remove-cstr-calls`

Now let's take a look at the existing refactoring tool `remove-cstr-calls`
on which we will base our refactoring tool.  This will give us an introduction
to the library we're going to use to access the parsed C++ source file
and associate replacement text with existing source text.

Open up `llvm/tools/clang/tools/extra/remove-cstr-calls/RemoveCStrCalls.cpp`
in your editor.  At the top of the file we see a block comment summarizing
the tool:

```C++
//  This file implements a tool that prints replacements that remove redundant
//  calls of c_str() on strings.
//
//  Usage:
//  remove-cstr-calls <cmake-output-dir> <file1> <file2> ...
//
//  Where <cmake-output-dir> is a CMake build directory in which a file named
//  compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//  CMake to get this output).
//
//  <file1> ... specify the paths of files in the CMake source tree. This path
//  is looked up in the compile command database. If the path of a file is
//  absolute, it needs to point into CMake's source tree. If the path is
//  relative, the current working directory needs to be in the CMake source
//  tree and the file must be in a subdirectory of the current working
//  directory. "./" prefixes in the relative files will be automatically
//  removed, but the rest of a relative path must be a suffix of a path in
//  the compile command line database.
//
//  For example, to use remove-cstr-calls on all files in a subtree of the
//  source tree, use:
//
//    /path/in/subtree $ find . -name '*.cpp'|
//        xargs remove-cstr-calls /path/to/build
```

Our new tool will use the same command-line argument structure because
we will be performing the same sort of transformation on the source code.

### `main`: Startup

At the bottom of the file, you'll find the definition of `main` which
begins with code like this:

```C++
cl::opt<std::string> BuildPath(
  cl::Positional,
  cl::desc("<build-path>"));

cl::list<std::string> SourcePaths(
  cl::Positional,
  cl::desc("<source0> [... <sourceN>]"),
  cl::OneOrMore);

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::OwningPtr<CompilationDatabase> Compilations(
    tooling::FixedCompilationDatabase::loadFromCommandLine(argc, argv));
  cl::ParseCommandLineOptions(argc, argv);
  if (!Compilations) {
    std::string ErrorMessage;
    Compilations.reset(
           CompilationDatabase::loadFromDirectory(BuildPath, ErrorMessage));
    if (!Compilations)
      llvm::report_fatal_error(ErrorMessage);
    }
  tooling::RefactoringTool Tool(*Compilations, SourcePaths);
```

* `cl` is clang's namespace for command-line argument processing.
`BuildPath` and `SourcePaths` are the two inputs to this refactoring tool:
** `BuildPath` is where the tool will look for the compilation database
** `SourcePaths` is a list of source files to be refactored   
* `PrintStackTraceOnErrorSignal` is a helper function that dumps the
current stack trace if an unexpected signal occurs.
* `CompilationDatabase` contains clang's abstraction of compile switches
needed to properly compile a source file.  When refactoring C++ code, we
need to know everything about how that source file will be compiled in
order to know the proper definitions of all the text used in the source
file.  Otherwise, we will not be able to properly identify a piece of
text as a macro invocation, for instance.  We will look into the
compilation database in more detail.
* `cl::ParseCommandLineOptions` handles all the command-line options
that we might need to specify regarding include paths and so-on.
* `tooling::RefactoringTool` is the class in the tooling library that
gives us the infrastructure for performing source-to-source transformations
on source files and writing out the updated source file in place.

### `main`: Abstract Syntax Tree (AST) Matching

The next two lines declare a `MatchFinder` that we'll use to match nodes
in the AST and an instance of a callback class that will be invoked when
a matching node is found.  The callback class, `FixCStrCall`, is where the
meat of our refactoring tool resides and is defined at the top of the file.
The instance of the callback is connected to the list of source file
replacements managed by the `RefactoringTool`.

```C++
  ast_matchers::MatchFinder Finder;
  FixCStrCall Callback(&Tool.getReplacements());
```

Next, the first matcher is added to `Finder`.  Matchers with the tooling
library are specified using a builder style interface.  This allows the
matchers to be expressive in the form of a fluent API.  A summary of the
matcher API can be found in the
[AST Matchers Reference](http://clang.llvm.org/docs/LibASTMatchersReference.html)

This matcher looks for invocations of a `std::string` constructor with
two arguments where
the first argument is a call to `std::string::c_str()` and
the second argument is the default argument.
The second argument to string's constructor is an allocator used for
custom string allocation.
The default allocator simply obtains string memory from the heap using
`new`.
The refactoring tool doesn't want to match on strings that use
a custom allocator, so it insists the argument be the default argument.
`StringConstructor` and `StringCStrMethod` are named string constants
the provide the fully qualified names of the constructor and `c_str` methods.

```C++
  Finder.addMatcher(
      constructExpr(
          hasDeclaration(methodDecl(hasName(StringConstructor))),
          argumentCountIs(2),
          // The first argument must have the form x.c_str() or p->c_str()
          // where the method is string::c_str().  We can use the copy
          // constructor of string instead (or the compiler might share
          // the string object).
          hasArgument(
              0,
              id("call", memberCallExpr(
                  callee(id("member", memberExpr())),
                  callee(methodDecl(hasName(StringCStrMethod))),
                  on(id("arg", expr()))))),
          // The second argument is the alloc object which must not be
          // present explicitly.
          hasArgument(
              1,
              defaultArgExpr())),
      &Callback);
```

The second matcher is specific to the clang codebase, which defines two
string-like classes
[`StringRef` and `Twine`](http://llvm.org/docs/ProgrammersManual.html#passing-strings-the-stringref-and-twine-classes).
The matching criteria is similar to that of the first matcher:

```C++
  Finder.addMatcher(
      constructExpr(
          // Implicit constructors of these classes are overloaded
          // wrt. string types and they internally make a StringRef
          // referring to the argument.  Passing a string directly to
          // them is preferred to passing a char pointer.
          hasDeclaration(methodDecl(anyOf(
              hasName("::llvm::StringRef::StringRef"),
              hasName("::llvm::Twine::Twine")))),
          argumentCountIs(1),
          // The only argument must have the form x.c_str() or p->c_str()
          // where the method is string::c_str().  StringRef also has
          // a constructor from string which is more efficient (avoids
          // strlen), so we can construct StringRef from the string
          // directly.
          hasArgument(
              0,
              id("call", memberCallExpr(
                  callee(id("member", memberExpr())),
                  callee(methodDecl(hasName(StringCStrMethod))),
                  on(id("arg", expr())))))),
      &Callback);
```

The last line of `main` does all the work now that everything has been
wired up.  `runAndSave` parses the source files to build the AST,
finds matching ndoes in the AST and invokes the callback for each match,
building a list of source text replacements.  Those replacements are
applied to the source file in place and the updated source file is
written out.

```C++
  return Tool.runAndSave(newFrontendActionFactory(&Finder));
```

## Bootstrapping `remove-void-args`

If imitation is the sincerest form of flattery, then let us pay a supreme
complient to the authors of `remove-cstr-calls` and begin by copying their
entire implementation and changing the matchers.

1. Make a copy of the directory `llvm/tools/clang/tools/extra/remove-cstr-calls`
and name it `llvm/tools/clang/tools/extra/remove-void-args`.
2. Rename `RemoveCStrCalls.cpp` to `RemoveVoidArgs.cpp`
3. Edit `CMakeLists.txt` and:
3.1 change `remove-cstr-calls` to `remove-void-args`
3.2 change `RemoveCStrCalls.cpp` to `RemoveVoidArgs.cpp`
4. Edit `Makefile` and change `remove-cstr-calls` to `remove-void-args`
5. Edit `llvm/tools/clang/tools/extra/CMakeLists.txt` and add the line
```
add_subdirectory(remove-void-args)
```
after the line for `remove-cstr-calls`.
6. Edit `llvm/tools/clang/tools/extra/Makefile` and add `remove-void-args`
to the definition of `PARALLEL_DIRS` after the existing `remove-cstr-calls`.

Now we have a copy of `remove-cstr-calls` under a new name, `remove-void-args`.
Let's test that the build is creating our new refactoring tool:

* On **Windows**, rerun CMake (`cmake -G "Visual Studio 11" ..\llvm`) and build `LLVM.sln`
The LLVM solution should contain a project for your new refactoring tool.
* On **Linux**, rerun configure (`../llvm/configure`) and build with `make`.
