#include "testlib.h"
#include <string>
#include <vector>
#include <sstream>

using namespace std;

/* 
 * 去除每行的末尾空白字符
 * 去除文件末尾空白行
 * */

void del_line_back_space(string &str){
    while(!str.empty()){
        string::iterator it = str.end()-1;
        if( *it == '\n' || *it == ' ' || *it == '\r')
            str.erase(it);
        else
            break;
    }
}

int main(int argc, char * argv[])
{
    setName("compare files as sequence of lines");
    registerTestlibCmd(argc, argv);

    std::string strAnswer;

    int n = 0;
    while (!ans.eof()) 
    {
        std::string j = ans.readString();
        
        del_line_back_space(j);     //去除末尾空白
        if (j.empty() && ans.eof()) // ans 读到最后一个空白行
          break;

        strAnswer = j;
        std::string p = ouf.readString();
        del_line_back_space(p);

        n++;

        if (j != p)
            quitf(_wa, "%d%s lines differ - expected: '%s', found: '%s'", n, englishEnding(n).c_str(), compress(j).c_str(), compress(p).c_str());
    }
    
    if (n == 1)
        quitf(_ok, "single line: '%s'", compress(strAnswer).c_str());
    
    quitf(_ok, "%d lines", n);
}
