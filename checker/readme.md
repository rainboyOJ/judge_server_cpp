## 安装

```
rm -rf build && mkdir build && cd build && cmake ..
make || exit 1
make install
```
## oj 输出文件正确判断

## 安装

```
mkdir build
cd build && cmake ..
make && sudo make install
```

## 使用

```
[checker ] <in> <out> <ans>
```

 - in 输入文件
 - out 用户输出文件
 - ans 标准输出文件

### fcmp2 

 - 按行比较文件
 - 过滤每行的末尾的空格
 - 不比较文件末尾的空白行
