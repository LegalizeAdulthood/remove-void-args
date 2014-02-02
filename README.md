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
* clang Compiler Runtime 3.4: <http://llvm.org/releases/3.4/compiler-rt-3.4.src.tar.gz>
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
3. Unpack clang Compiler Runtime 3.4, you should have a directory named `compiler-rt-3.4`
4. Unpack clang Tools Extra 3.4, you should have a directory named `clang-tools-extra-3.4`
5. Move the `clang-3.4` directory to `llvm-3.4/tools/clang`
6. Move the `compiler-rt-3.4` directory to `llvm-3.4/projects/compiler-rt`
6. Move the `clang-tools-extra-3.4` directory to `llvm-3.4/tools/clang/tools/extra`

### Building clang

The clang compiler code base is designed to be bootstrapped using the
native compiler to the system.  Follow the instructions on clang's
Getting Started page: <http://clang.llvm.org/get_started.html> for
your operating system with one difference -- since we have downloaded
source packages, we don't need to checkout source code from subversion.

1. Create a directory for build outputs called `build`, as a sibling of `llvm-3.4`
2. `cd build`
3. For Linux/Macintosh users: `../llvm/configure; make`
4. For Windows users: `cmake -G "Visual Studio 11" ..\llvm` and then build
`LLVM.sln` from within Visual Studio 2012 or later.
