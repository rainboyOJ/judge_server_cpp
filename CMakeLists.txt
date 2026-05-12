cmake_minimum_required(VERSION 3.10)  # 指定CMake的最低版本

# 设置项目名称和版本
project(boxTest VERSION 1.0)

option(SJUDGER_ENABLE_SECCOMP "Build sjudger with seccomp support" ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)          # 可以根据需要调整标准版本
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(bt_build_settings INTERFACE)
target_include_directories(bt_build_settings INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/sjudge"
)
target_compile_definitions(bt_build_settings INTERFACE
    PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    $<$<CONFIG:Debug>:MUDEBUG>
)
target_link_libraries(bt_build_settings INTERFACE "-Wl,--copy-dt-needed-entries")

add_library(bt_common STATIC
    src/common/Config.cpp
    src/common/Logger.cpp
    src/common/Timestamp.cpp
    src/common/utils.cpp
)
target_link_libraries(bt_common PUBLIC bt_build_settings)

add_library(bt_protocol STATIC
    src/protocol/JudgeProtocol.cpp
)
target_link_libraries(bt_protocol PUBLIC bt_build_settings bt_common)

set(SJUDGER_SECCOMP_SOURCE sjudge/SeccompPolicyStub.cpp)

add_library(bt_runner STATIC
    sjudge/ChildSetup.cpp
    sjudge/ConfigValidator.cpp
    sjudge/ExternalSjudgeCompat.cpp
    sjudge/ParentMonitor.cpp
    sjudge/ResultMapper.cpp
    sjudge/sjudge_call.cpp
    ${SJUDGER_SECCOMP_SOURCE}
    src/runner/RunnerCompileSupport.cpp
    src/runner/RunnerExecutionSupport.cpp
    src/runner/CppRunner.cpp
    src/runner/PythonRunner.cpp
    src/runner/RunnerFactory.cpp
)
target_link_libraries(bt_runner PUBLIC bt_build_settings bt_common)

if(SJUDGER_ENABLE_SECCOMP)
    target_compile_definitions(bt_runner PUBLIC SJUDGER_ENABLE_SECCOMP=1)
endif()

add_library(bt_pipeline STATIC
    src/pipeline/JudgeCore.cpp
    src/pipeline/ResultStore.cpp
    src/pipeline/SubmissionService.cpp
)
target_link_libraries(bt_pipeline PUBLIC bt_build_settings bt_common bt_runner)

add_library(bt_dispatch STATIC
    src/dispatch/JudgeWorkerPool.cpp
    src/dispatch/SubmissionQueue.cpp
)
target_link_libraries(bt_dispatch PUBLIC bt_build_settings bt_common bt_pipeline)

add_library(bt_network STATIC
    src/network/AckBarrier.cpp
    src/network/Buffer.cpp
    src/network/ClientSockets.cpp
    src/network/ConnectionRegistry.cpp
    src/network/ConnectionSlot.cpp
    src/network/QueryRequestHandler.cpp
    src/network/ReplyChannel.cpp
    src/network/SubmissionEventResponder.cpp
    src/network/SubmissionRequestHandler.cpp
    src/network/TcpServer.cpp
)
target_link_libraries(bt_network PUBLIC
    bt_build_settings
    bt_common
    bt_dispatch
    bt_pipeline
    bt_protocol
)

add_library(boxTest INTERFACE)
target_link_libraries(boxTest INTERFACE
    bt_build_settings
    bt_common
    bt_protocol
    bt_runner
    bt_pipeline
    bt_dispatch
    bt_network
)

add_executable(judge_server src/main.cpp)
target_link_libraries(judge_server PRIVATE boxTest)


# 如果你有测试代码，可以使用CMake的测试框架（如CTest）进行构建
enable_testing()
# 添加测试目标
add_subdirectory(test)
