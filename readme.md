实现boxtest功能


组件

1. Logger 这个组件很简单,就是进行日志的输出

1. cachebox
2. pointTest
3. testBox 测试盒子,接收测试数据,返回测试结果

## 编译

```bash
mkdir build && cd build
cmake .. 
# add Debug 
# cmake .. -DCMAKE_BUILD_TYPE=Debug
# add Release
# cmake.. -DCMAKE_BUILD_TYPE=Release
make
```
