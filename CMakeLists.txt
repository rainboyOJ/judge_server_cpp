cmake_minimum_required(VERSION 3.10)  # 指定CMake的最低版本

# 设置项目名称和版本
project(boxTest VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)          # 可以根据需要调整标准版本
set(CMAKE_CXX_STANDARD_REQUIRED True)

#  from this https://github.com/protocolbuffers/protobuf/issues/14500
# 解决 protobuf 链接的问题
# set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_STATIC_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")

# 添加头文件目录
include_directories(include)

# 查找源文件并添加可执行文件
file(GLOB SOURCES "src/*.cpp")
add_library(${CMAKE_PROJECT_NAME} STATIC ${SOURCES})
# target_link_libraries(${CMAKE_PROJECT_NAME} "protobuf")

# 如果你有测试代码，可以使用CMake的测试框架（如CTest）进行构建
enable_testing()
# 添加测试目标
add_subdirectory(test)

