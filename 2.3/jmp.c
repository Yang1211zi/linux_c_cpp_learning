#include <setjmp.h>
#include<stdio.h>

jmp_buf env;//保存程序某一时刻的执行现场
/*
 *如果想设定多个跳转位置，可以设置多个env
 */
jmp_buf env2;

void func(int n) {
    printf("%d\n",n);
    longjmp(env,++n);//执行现场+设定返回参数值
}

int main() {
    int ret1=setjmp(env);//把当前执行现场保存到env，在返回一个整数
    if(ret1==0) {
        func(0);
    }
    if(ret1==1) {
        func(1);
    }
    if(ret1==2) {
        func(2);
    }
    int ret2=setjmp(env2);
    if(ret1==3) {
        longjmp(env2,1);
    }
    if (ret2==2) {
        printf("%d\n",ret2);
    }
    return 0;
}