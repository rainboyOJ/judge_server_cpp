#include <iostream>
#include <arpa/inet.h>

#include "../include/judgeInfo.h"

using namespace std;

void print(int &a) {
    for(int i = 0 ;i<=3;i++) {
        char * c = (char*)&a + i;
        int t = *c;
        // std::cout << std::hex << c << ": " << std::hex << t  << "\n";
        printf("0x%llx : 0x%x\n",c,t);
    }

}

//把整数 int 或long long 转换成字符串
template<typename T>
std::string serializeInt(const T &t){
    std::string ret;
    int size = sizeof(T);
    if constexpr ( std::is_same<T, int>::value  || std::is_same<T, unsigned int>::value )
    {
        T x = htonl(t);
        for(int i=0; i<size; i++){
            ret += x & 0xff;
            x >>= 8;
        }
    }
    else if constexpr ( std::is_same<T, long long>::value  || std::is_same<T, unsigned long long>::value )
    {
        T x = htonll(t);
        for(int i=0; i<size; i++){
            ret += x & 0xff;
            x >>= 8;
        }
    }
    return std::move(ret);
}

//转成整数 int 或long long
template<typename T>
T deserializeInt(const char * src){
    int size = sizeof(T);
    T x = 0;
    for (int i = size-1; i >=0; i--)
    {
        x <<= 8;
        x |= src[i];
    }
    if constexpr ( std::is_same<T, int>::value  || std::is_same<T, unsigned int>::value )
    {
        return ntohl(x);
    }
    else if constexpr ( std::is_same<T, long long>::value  || std::is_same<T, unsigned long long>::value )
    {
        return ntohll(x);
    }
}

void print(const std::string &s ) {
    for(int i =0 ;i< s.size();i++)
        printf("0x%x ",s[i]);
    printf("\n");
}


int main (int argc, char *argv[]) {
    std::cout << 1 << "\n";
    int a = 0x12345678;
    print(a);
    printf("%x\n",a);
    printf("%d\n",a);

    printf("\n\n");
    int b = htonl(a);
    print(b);

    printf("\n\n");
    std::string s1 = serializeInt(a);
    print(s1);

    int c = deserializeInt<int>(s1.c_str());
    print(c);
    printf("%x\n",c);
    printf("%d\n",c);
    return 0;
}
