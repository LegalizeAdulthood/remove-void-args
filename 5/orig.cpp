#include <cstdio>

int foo(void);

int foo(void)
{
    return 0;
}

void bar()
{
}

typedef void (function_ptr)(void);

class gronk
{
public:
    void foo();
    void bar(void);
    gronk(void);
    ~gronk(void);
};

void (*f1)(void);
void (*f2)(void) = nullptr;
void (*f3)(void) = bar;

void (gronk::*p1)();
void (gronk::*p2)(void);
void (gronk::*p3)(void) = &gronk::foo;
typedef void (gronk::*member_function_ptr)(void);

void gronk::foo()
{
}

void gronk::bar(void)
{
}

gronk::gronk(void)
{
}

gronk::~gronk(void)
{
}
