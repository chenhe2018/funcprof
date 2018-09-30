#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <stdio.h>

#define PROF

#include "./FuncProf.h"

using namespace std;

class A {
public:
    void funcA(int x);
};

void funcC() {
    PROF_FUNC_ENTRY();
    sleep(1);
    PROF_FUNC_EXIT();
}

void funcB() {
    PROF_FUNC_ENTRY();
    sleep(2);
    PROF_FUNC_EXIT();
}

void A::funcA(int x) {
    funcB();
    funcC();
}

int main(int argc, char *argv[]) {
    A a;
    int i=0;
    while (1) {
        PROF_NF_BEGIN("::funcA");
        //a.funcA(10);
        printf("%d\n",i++);
        PROF_NF_END("::funcA");
    }
    return 0;
}