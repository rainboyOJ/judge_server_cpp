#include "minheap.hpp"
#include <iostream>
#include <random>
#include <algorithm>
using std::cout;


std::random_device rd;
std::default_random_engine __rnd(rd());

// 预先定义的随机数生成器 https://en.cppreference.com/w/cpp/numeric/random
std::mt19937 mtrnd(rd());

// 每一次产生指定范围内的随机数
// std::uniform_int_distribution
// https://en.cppreference.com/w/cpp/numeric/random/uniform_int_distribution

//生成[l,r]之间的随机整数
int rnd(int l,int r) {
    return  mtrnd() % (r-l+1) + l;
}


int a[1000];

int main (int argc, char *argv[]) {

    //1. 创建
    MinHeap myhead(100);
    myhead.init();

    //1.输出
    while (!myhead.empty()) {
        std::cout<< myhead.top() << " ";
        myhead.pop();
    }
    std::cout << "\n";

    int n = rnd(10,100);

    std::cout << "get " << n << " random numbers: \n";
    for(int i =1;i <=n;i++) {
        a[i] = rnd(1,1000);
        std::cout << a[i] << " ";
        myhead.add(a[i]);
    }
    
    std::sort(a+1,a+1+n);
    std::cout << "after sort : " << "\n";
    for(int i =1;i <=n;i++) {
        std::cout << a[i] << " ";
    }
    std::cout  << "\n";

    std::cout << "cout heap sort : " << "\n";
    int right_flag = 1;
    int idx = 1;
    while (!myhead.empty()) {
        int  t =  myhead.top();
        if( t != a[idx++])
            right_flag = 0;
        std::cout<< t << " ";
        myhead.pop();
    }
    std::cout  << "\n";
    std::cout << (right_flag ? "Yes" : " No" )<< "\n";



    
    return 0;
}
