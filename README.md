# `remove-void-args`

In this tutorial, we'll walk through the steps of implementing a refactoring
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
`add_subdirectory(remove-void-args)` after the line for `remove-cstr-calls`.
6. Edit `llvm/tools/clang/tools/extra/Makefile` and add `remove-void-args`
to the definition of `PARALLEL_DIRS` after the existing `remove-cstr-calls`.

Now we have a copy of `remove-cstr-calls` under a new name, `remove-void-args`.
Let's test that the build is creating our new refactoring tool:

* On **Windows**, rerun CMake (`cmake -G "Visual Studio 11" ..\llvm`) and build `LLVM.sln`
The LLVM solution should contain a project for your new refactoring tool.
* On **Linux**, rerun configure (`../llvm/configure`) and build with `make`.

## Understanding clang's AST

Before we can start writing matchers for our `(void)` argument list,
we need to understand the AST that clang builds for our source text.
Fortunately for us, now that we've built clang, we can instruct it to
dump the AST for a source file.

Edit the new file `test.cpp` and insert the following text:

```C++
int foo(void) {
    return 0;
}

int bar() {
    return 0;
}

int feezle(int i) {
    return 0;
}
```

Then tell clang to dump the AST for us with `-ast-dump` to get:

```
> clang -Xclang -ast-dump -fsyntax-only dump.cpp
TranslationUnitDecl 0x469850 <<invalid sloc>>
|-TypedefDecl 0x469b40 <<invalid sloc>> __builtin_va_list 'char *'
|-CXXRecordDecl 0x469b70 <<built-in>:28:1, col:7> class type_info
|-FunctionDecl 0x469c60 <dump.cpp:1:1, line:3:1> foo 'int (void)'
| `-CompoundStmt 0x469d00 <line:1:15, line:3:1>
|   `-ReturnStmt 0x469cf0 <line:2:5, col:12>
|     `-IntegerLiteral 0x469cd0 <col:12> 'int' 0
|-FunctionDecl 0x469d40 <line:5:1, line:7:1> bar 'int (void)'
| `-CompoundStmt 0x469de0 <line:5:11, line:7:1>
|   `-ReturnStmt 0x469dd0 <line:6:5, col:12>
|     `-IntegerLiteral 0x469db0 <col:12> 'int' 0
`-FunctionDecl 0x469e90 <line:9:1, line:11:1> feezle 'int (int)'
  |-ParmVarDecl 0x469e10 <line:9:12, col:16> i 'int'
  `-CompoundStmt 0x469f38 <col:19, line:11:1>
    `-ReturnStmt 0x469f28 <line:10:5, col:12>
      `-IntegerLiteral 0x469f08 <col:12> 'int' 0
```

On your console, the output may be colored to indicate distinctions
between `Decl` entries, `Stmt` entries, types, source file locations, etc.  
The first thing you notice is that everything is inside a
`TranslationUnitDecl`.
The compilation and link model for C++ says that each source file
corresponds to a distinct translation unit.
Next we see a couple of internal declarations provided by clang and
then three `FunctionDecl` entries for the functions we defined.
Note that the first two `FunctionDecl` entries have no arguments and
the third has a single `ParmVarDecl` for it's argument.

Do you notice something odd about the two `FunctionDecl` entries?
Only the `foo` function provided the `(void)` argument list, but
both entries reported by `-ast-dump` list the `(void)` signature.
Why is that?

When clang creates the AST, it normalizes the representation so that two
different ways of expressing the same fact in source code have a single
representation in the AST.

Does this mean we won't be able to distinguish `(void)` from `()` in
our AST matching?  Fortunately, no, because we can access the source
text associated with any particular AST node from inside the matcher.

## The Compilation Database

Remember that compilation database we mentioned in our discussion of
`main`?  We need one of these in order to run our refactoring tool on
a source file.  The database consists of a JSON file that provides the
compilation command for each file.  On a unix system, CMake can generate
the compilation database with the following command from the `build`
directory:

```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS ../llvm
```

On a Windows system, we have to hack one together as the compile
commands support in CMake only works on unix.

Since we want to experiment with our tool on a single test file,
we can just create one in a text editor.  Create a
file called `compile_commands.json` with the following contents.

For **Windows**:
```JSON
[
    {
        "directory": "D:/Code/clang/llvm/tools/clang/tools/extra/remove-void-args",
        "command": "CL.exe /c /I\"D:/Code/clang/tools/clang/tools/extra/remove-void-args\" \"D:/Code/clang/llvm/tools/clang/tools/extra/remove-void-args/test.cpp\"",
        "file": "test.cpp"
    }
]
```

The directories must be separated with slashes and not backslashes, so
if you paste in a Windows path with backslashes, remember to replace them
all with slashes.

For **Linux**:
```JSON
[
    {
        "directory": "/Code/clang/llvm/tools/clang/tools/extra/remove-void-args",
        "command": "g++ -c -I/Code/clang/tools/clang/tools/extra/remove-void-args /Code/clang/llvm/tools/clang/tools/extra/remove-void-args/test.cpp",
        "file": "test.cpp"
    }
]
```

Adjust the contents of the file to match the corresponding location to
your `remove-void-args` directory and the file `test.cpp`.

Verify your `compile_commands.json` file is working correctly by running
`remove-void-args`.  This is most easily done by having the `build/bin`
directory (`build\bin\Debug` on **Windows**) in your path and making
`llvm/tools/clang/tools/extra/remove-void-args` as your current directory.

```
> remove-void-args . test.cpp
warning: /c: 'linker' input unused
warning: /ID:/Code/clang/llvm/tools/clang/tools/extra/remove-void-args: 'linker' input unused
```

The warnings on **Windows** are harmless.  Since test.cpp doesn't contain
any code matched by the `remove-cstr-calls` algorithm we copied, the
contents of `test.cpp` remain unchanged.

If you see this error message, it's because you didn't replace `\` with
`/` in your `compile_commands.json` file.

```
> remove-void-args . dump.cpp
YAML:6:6: error: Unrecognized escape code!
    }
     ^
Skipping D:\Code\clang\llvm\tools\clang\tools\extra\remove-void-args\test.cpp. Command line not found.
```

Now we're ready to edit `RemoveVoidArgs.cpp` and start matching AST
nodes and refactoring the source file.

## Exploring Matched Function Definitions

To get a feel for how all this works, let's add some simple code that
matches a `FunctionDecl` and prints out information about the matched
node.  Open `RemoveVoidArgs.cpp` and add the following includes at the
top:

```C++
#include <iostream>
#include <sstream>
```

Delete the functions `needParensAfterUnaryOperator` and `formatDereference`
and the string constants `StringConstructor` and `StringCStrMethod`
since they are used by `remove-cstr-calls`, but we won't need them.
We'll keep the `getText` function since it will give us the source text
associated with any matched node.

Replace the class `FixCStrCall` with this class:

```C++
class FixVoidArg : public ast_matchers::MatchFinder::MatchCallback {
 public:
  FixVoidArg(tooling::Replacements *Replace)
      : Replace(Replace) {}

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    BoundNodes Nodes = Result.Nodes;
    SourceManager const *SM = Result.SourceManager;
    if (FunctionDecl const *const Function = Nodes.getNodeAs<FunctionDecl>("fn")) {
        std::string const Text = getText(*SM, *Function);
        if (Text.length() > 0) {
            std::string::size_type OpenBrace = Text.find_first_of('{');
            if (OpenBrace == std::string::npos) {
                return;
            }
            std::string::size_type EndOfDecl = Text.find_last_of(')', OpenBrace) + 1;
            std::string Decl = Text.substr(0, EndOfDecl);
            if (Decl.length() > 6 && Decl.substr(Decl.length()-6) == "(void)") {
                std::cout << "Void Definition : " << getLocation(SM, Function) << Decl << "\n";
            }
        }
    }
  }

 private:
    std::string getLocation(SourceManager const *SM, FunctionDecl const* const Function) {
        std::ostringstream location;
        std::pair<FileID, unsigned> decomposed = SM->getDecomposedLoc(Function->getLocStart());
        if (FileEntry const *entry = SM->getFileEntryForID(decomposed.first)) {
            std::string fileName = entry->getName();
            location << fileName.substr(fileName.find_last_of("\\/") + 1);
        }
        location << "(" << SM->getLineNumber(decomposed.first, decomposed.second) << "): ";
        return location.str();
    }
  tooling::Replacements *Replace;
};
```

Replace the `Callback` and matchers added with the following:

```C++
  FixVoidArg Callback(&Tool.getReplacements());
  Finder.addMatcher(functionDecl(parameterCountIs(0)).bind("fn"), &Callback);
````

We're matching function declarations (and definitions) with zero arguments
and binding them to the name *fn* for use by the callback.  The callback
gets the source text for the function definition and uses a string matching
heuristic to find the source text matching the argument list by looking for
the first `{` and then the last `)` before that.

Rebuild `remove-void-args` and run it on `test.cpp` again to see the results:

```
Void Definition : test.cpp(1): int foo(void)
```

You'll find this version of the code in [directory 1](1).

## Exploring A Realistic Source File

Our test source file, `test.cpp`, isn't very realistic.  It doesn't include
any headers, for instance.  Let's make it more realistic by adding the
following line to the top of `test.cpp`:

```C++
#include <cstdio>
```

Now run `remove-void-args` again and see the results:

```
> remove-void-args . test.cpp
Void Definition : crtdefs.h(571): void __cdecl _invalid_parameter_noinfo(void)
Void Definition : crtdefs.h(572): __declspec(noreturn) void __cdecl _invalid_parameter_noinfo_noreturn(void)
Void Definition : stdio.h(129): FILE * __cdecl __iob_func(void)
Void Definition : stdio.h(184): int __cdecl _fcloseall(void)
Void Definition : stdio.h(196): int __cdecl _fgetchar(void)
Void Definition : stdio.h(217): int __cdecl _flushall(void)
Void Definition : stdio.h(255): int __cdecl getchar(void)
Void Definition : stdio.h(256): int __cdecl _getmaxstdio(void)
Void Definition : stdio.h(289): int __cdecl _rmtmp(void)
Void Definition : stdio.h(302): unsigned int __cdecl _get_output_format(void)
Void Definition : stdio.h(372): int __cdecl _get_printf_count_output(void)
Void Definition : stdio.h(422): wint_t __cdecl _fgetwchar(void)
Void Definition : stdio.h(426): wint_t __cdecl getwchar(void)
Void Definition : test.cpp(3): int foo(void)
```

Uh oh, here's a wrinkle we hadn't anticipated.  C++'s backwards compatability
with C is bringing in function signatures we didn't want to care about.  We
could filter based on the source file name, but a better idea is to exclude
anything that is declared with C linkage, i.e. declared `extern "C"`.  Add
the following code in the callback just before the call to `getText`:

```
if (Function->isExternC()) {
    return;
}
```

Rebuild and run `remove-void-args` and you will now see the following
results:

```
> remove-void-args . test.cpp
Void Definition : test.cpp(3): int foo(void)
```

You'll find this version of the code in [directory 2](2).

## Exploring Matched Function Declarations

So far, we are only handling function definitions because our matching
heuristic relies on the presence of an open brace, `{`, to identify
the arguments in the source text.  Let's add a function declaration
to our test file.  Add this declaration for `foo` before the definition
of `foo`

Let's add a function declaration to `test.cpp` by adding the following
line:

```C++
int foo(void);
```

We run `remove-void-args` and get the following output:

```
Void Definition : test.cpp(5): int foo(void)
```

We need a way to distinguish between a function declaration, where no
opening brace will be present, and a function definition, so that we'll
know how to extract the appropriate source text for matching.  Change
the `run` method to the following:

```C++
virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    BoundNodes Nodes = Result.Nodes;
    SourceManager const *SM = Result.SourceManager;
    if (FunctionDecl const *const Function = Nodes.getNodeAs<FunctionDecl>("fn")) {
        if (Function->isExternC()) {
            return;
        }
        std::string const Text = getText(*SM, *Function);
        if (!Function->isThisDeclarationADefinition()) {
            if (Text.length() > 6 && Text.substr(Text.length()-6) == "(void)") {
                std::cout << "Void Declaration: " << getLocation(SM, Function) << Text << "\n";
            }
        } else if (Text.length() > 0) {
            std::string::size_type EndOfDecl = Text.find_last_of(')', Text.find_first_of('{')) + 1;
            std::string Decl = Text.substr(0, EndOfDecl);
            if (Decl.length() > 6 && Decl.substr(Decl.length()-6) == "(void)") {
                std::cout << "Void Definition : " << getLocation(SM, Function) << Decl << "\n";
            }
        }
    }
}
```

Now that we can distinguish between a declaration and a definition,
we don't need to guard against a missing open brace in the definition.
Building `remove-void-args` and running it on `test.cpp` now yields the
following output:

```
> remove-void-args . test.cpp
Void Declaration: test.cpp(3): int foo(void)
Void Definition : test.cpp(5): int foo(void)
```

You'll find this version of the code in [directory 3](3).

## Replacing Matched Function Declarations and Definitions

OK, enough exploration, let's do some refactoring!  Once we've identified
the source text we want to replace, we add a replacement to the replacements
list given to the constructor of our callback class.  The replacement is
the original text of the node, with `(void)` replaced by `()`.  Change
the class `FixVoidArg` to the following:

```C++
class FixVoidArg : public ast_matchers::MatchFinder::MatchCallback {
 public:
  FixVoidArg(tooling::Replacements *Replace)
      : Replace(Replace) {}

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    BoundNodes Nodes = Result.Nodes;
    SourceManager const *SM = Result.SourceManager;
    if (FunctionDecl const *const Function = Nodes.getNodeAs<FunctionDecl>("fn")) {
        if (Function->isExternC()) {
            return;
        }
        std::string const Text = getText(*SM, *Function);
        if (!Function->isThisDeclarationADefinition()) {
            if (Text.length() > 6 && Text.substr(Text.length()-6) == "(void)") {
                std::string const NoVoid = Text.substr(0, Text.length()-6) + "()";
                Replace->insert(Replacement(*Result.SourceManager, Function, NoVoid));
            }
        } else if (Text.length() > 0) {
            std::string::size_type EndOfDecl = Text.find_last_of(')', Text.find_first_of('{')) + 1;
            std::string Decl = Text.substr(0, EndOfDecl);
            if (Decl.length() > 6 && Decl.substr(Decl.length()-6) == "(void)") {
                std::string NoVoid = Decl.substr(0, Decl.length()-6) + "()" + Text.substr(EndOfDecl);
                Replace->insert(Replacement(*Result.SourceManager, Function, NoVoid));
            }
        }
    }
  }

 private:
  tooling::Replacements *Replace;
};
```

Although not strictly necessary, you can also remove the includes of
`<iostream>` and `<sstream>`, since we only needed them to print out
some diagnostic information.

Rebuild `remove-void-args` and run it on `test.cpp`, which is updated
to the following contents:

```C++
#include <cstdio>

int foo();

int foo() {
    return 0;
}

int bar() {
    return 0;
}

int feezle(int i) {
    return 0;
}
```

Congratulations!

We have successfully refactored a declaration and a definition, while
leaving unrelated functions unchanged.

You'll find this version of the code in [directory 4](4).

## Handling `typedef` Statements

Now let's look at handling `typedef` statements.  Add the following code
to the end of `test.cpp`:

```C++
typedef int int_function(void);

typedef int (*int_function_ptr)(void);
```

Dump the syntax tree of the test file so we can see how clang treats a
`typedef` statement:

```
TranslationUnitDecl 0xfdc910 <<invalid sloc>>
...
|-FunctionDecl 0x6b6a8d0 <line:13:1, line:15:1> feezle 'int (int)'
| |-ParmVarDecl 0x6b6a870 <line:13:12, col:16> i 'int'
| `-CompoundStmt 0x6b6a978 <col:19, line:15:1>
|   `-ReturnStmt 0x6b6a968 <line:14:5, col:12>
|     `-IntegerLiteral 0x6b6a948 <col:12> 'int' 0
|-TypedefDecl 0x6b6ab30 <line:17:1, col:30> int_function 'int (void)'
`-TypedefDecl 0x6b6ac20 <line:19:1, col:37> int_function_ptr 'int (*)(void)'
```

The `TypedefDecl` node in the AST represents the `typedef` statements
that we added to our test file.  A quick perusal of the
[AST Matchers Reference](http://clang.llvm.org/docs/LibASTMatchersReference.html)
doesn't seem to provide any direct matcher for a `TypedefDecl` node.
However, we can use the `namedDecl` matcher to match any named declaration,
including a typedef.  We can then check in the callback to see if the
result of `getAsNode<TypedefDecl>` returns a non-zero pointer to identify
the named node as a `TypedefDecl`.

Change the `run` method of the callback to:

```C++
  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    BoundNodes Nodes = Result.Nodes;
    SourceManager const *SM = Result.SourceManager;
    if (FunctionDecl const *const Function = Nodes.getNodeAs<FunctionDecl>("fn")) {
        if (Function->isExternC()) {
            return;
        }
        std::string const Text = getText(*SM, *Function);
        if (!Function->isThisDeclarationADefinition()) {
            if (Text.length() > 6 && Text.substr(Text.length()-6) == "(void)") {
                std::string const NoVoid = Text.substr(0, Text.length()-6) + "()";
                Replace->insert(Replacement(*Result.SourceManager, Function, NoVoid));
            }
        } else if (Text.length() > 0) {
            std::string::size_type EndOfDecl = Text.find_last_of(')', Text.find_first_of('{')) + 1;
            std::string Decl = Text.substr(0, EndOfDecl);
            if (Decl.length() > 6 && Decl.substr(Decl.length()-6) == "(void)") {
                std::string NoVoid = Decl.substr(0, Decl.length()-6) + "()" + Text.substr(EndOfDecl);
                Replace->insert(Replacement(*Result.SourceManager, Function, NoVoid));
            }
        }
    } else if (TypedefDecl const *const Typedef = Nodes.getNodeAs<TypedefDecl>("td")) {
        std::string const Text = getText(*SM, *Typedef);
        if (Text.length() > 6 && Text.substr(Text.length()-6) == "(void)") {
            std::string const NoVoid = Text.substr(0, Text.length()-6) + "()";
            Replace->insert(Replacement(*Result.SourceManager, Typedef, NoVoid));
        }
    }
  }
```

Add a new matcher to `Finder`:

```C++
  Finder.addMatcher(namedDecl().bind("td"), &Callback);
```

Now when we run `remove-void-args . test.cpp`, the test file is transformed
into:

```C++
#include <cstdio>

int foo();

int foo() {
    return 0;
}

int bar() {
    return 0;
}

int feezle(int i) {
    return 0;
}

typedef int int_function();

typedef int (*int_function_ptr)();
```

and we've added support for typedefs to `remove-void-args`.

You'll find this version of the code in [directory 5](5).

## Member Functions

So far, we've only looked at C style constructs: free functions and
`typedef` statements.  Now let's see how well our tool handles member
functions.  Add the following code to `test.cpp`:

```C++
class gronk {
public:
    gronk(void);
    ~gronk(void);
    void foo(void);
    void bar();
};

gronk::gronk(void) {
}

gronk::~gronk(void) {
}

void gronk::foo(void) {
}

void gronk::bar() {
}
```

Trying out `remove-void-args` on `test.cpp` to see how well we're doing
with the existing code, we get:

```C++
class gronk {
public:
    gronk();
    ~gronk();
    void foo();
    void bar();
};

gronk::gronk() {
}

gronk::~gronk() {
}

void gronk::foo() {
}

void gronk::bar() {
}
```

Great!  We're already handling method declarations and definitions via
our matches to `FunctionDecl`.

You'll find this version of the code in [directory 6](6).
