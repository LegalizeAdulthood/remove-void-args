# remove-void-args

In this kata, we'll walk through the steps of implementing a refactoring
tool for C++ written _in_ C++ using the libraries provided with clang.
Our example refactoring tool will remove needless `(void)` argument list to
a function, method, or typedef.  Our refactoring tool will transform code
like this:

```
int foo(void)
{
    return 0;
}

void bar(void)
{
}

class gronk
{
public:
    void foo();
    void bar(void);
    gronk(void);
};

void gronk::foo()
{
}

void gronk::bar(void)
{
}

gronk::gronk(void)
{
}
```

into this:

```
int foo()
{
    return 0;
}

void bar(void)
{
}

class gronk
{
public:
    void foo();
    void bar();
    gronk();
};

void gronk::foo()
{
}

void gronk::bar()
{
}

gronk::gronk()
{
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
