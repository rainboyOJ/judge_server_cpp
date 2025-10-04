cmake_minimum_required(VERSION 3.10)  # 指定CMake的最低版本

# 设置项目名称和版本
project(boxTest VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)          # 可以根据需要调整标准版本
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 定义项目根目录的宏
add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# 添加头文件目
include_directories(include)
include_directories(sjudge)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")

# 检查是否是 Debug 模式
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DMUDEBUG)
endif()


# 查找源文件并添加可执行文件
file(GLOB SOURCES "src/*.cpp" "src/common/*.cpp" "src/test_process/*.cpp" "sjudge/sjudge_call.cpp")
add_library(${CMAKE_PROJECT_NAME} STATIC ${SOURCES})

# 编译主代码

add_executable(judge_server main.cpp)
target_link_libraries(judge_server ${CMAKE_PROJECT_NAME})


# 如果你有测试代码，可以使用CMake的测试框架（如CTest）进行构建
enable_testing()
# 添加测试目标
add_subdirectory(test)

