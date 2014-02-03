# remove-void-args

In this kata, we'll walk through the steps of implementing a refactoring
tool for C++ written _in_ C++ using the libraries provided with clang.
Our example refactoring tool will remove needless `(void)` argument list to
a function, method, or typedef.  Our refactoring tool will transform code
like this:

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

* LLVM 3.4 <http://llvm.org/releases/3.4/llvm-3.4.src.tar.gz>
* clang 3.4 <http://llvm.org/releases/3.4/clang-3.4.src.tar.gz>
* clang Tools Extra 3.4 <http://llvm.org/releases/3.4/clang-tools-extra-3.4.src.tar.gz>

Windows users will need `CMake` to prepare the source code for building.
Get `CMake` from <http://www.cmake.org/cmake/resources/software.html>.

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
Getting Started page: <http://clang.llvm.org/get_started.html> for
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
in your editor.  At the bottom of the file, you'll find the definition
of `main` which begins with code like this:

```C++
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
