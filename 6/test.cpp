#include <cstdio>

int foo(void);

int foo(void) {
    return 0;
}

int bar() {
    return 0;
}

int feezle(int i) {
    return 0;
}

typedef int int_function(void);

typedef int (*int_function_ptr)(void);

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
