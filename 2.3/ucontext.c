#include <dlfcn.h>

#include <stdio.h>
#include <ucontext.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

ucontext_t ctx[3];//为三个协程创建执行现场
ucontext_t main_ctx;//保存 main() 中调度器的执行现场

int count=0;

void func1() {
    while (count++<30) {
        printf("1\n");
        swapcontext(&ctx[0],&main_ctx);
        printf("2\n");
    }
}

void func2() {
    while (count++<30) {
        printf("3\n");
        swapcontext(&ctx[1],&main_ctx);
        printf("4\n");
    }
}

void func3() {
    while (count++<30) {
        printf("5\n");
        swapcontext(&ctx[2],&main_ctx);
        printf("6\n");
    }
}

int main() {
    //给协程的三个栈申请总空间
    char stack1[2048] = {0};
    char stack2[2048] = {0};
    char stack3[2048] = {0};

    //给协程申请执行现场并配置参数
    getcontext(&ctx[0]);
    ctx[0].uc_stack.ss_sp = stack1;//分配给协程的栈空间
    ctx[0].uc_stack.ss_size = sizeof(stack1);//栈空间对应的大小
    ctx[0].uc_link = &main_ctx;//协程函数执行完之后去哪
    makecontext(&ctx[0],func1,0);//配置协程函数与其参数;0代表没有参数，n代表有几个参数，  2,1,3)参数个数后面接具体的参数值
    //一般来说都是int型，即使传char也是先通过int传递后面在做强转

    getcontext(&ctx[1]);
    ctx[1].uc_stack.ss_sp=stack2;
    ctx[1].uc_stack.ss_size=sizeof(stack2);
    ctx[1].uc_link=&main_ctx;
    makecontext(&ctx[1],func1,0);

    getcontext(&ctx[2]);
    ctx[2].uc_stack.ss_sp=stack3;
    ctx[2].uc_stack.ss_size=sizeof(stack3);
    ctx[2].uc_link=&main_ctx;
    makecontext(&ctx[2],func2,0);

    while (count<30) {
        swapcontext(&main_ctx,&ctx[count%3]);
    }

    printf("\n");
    return 0;
}