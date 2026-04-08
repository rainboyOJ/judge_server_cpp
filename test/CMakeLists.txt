
# set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_STATIC_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")

# 只把真正的自动化测试注册进 CTest。
# `main.cpp`、`send_one_test.cpp`、`2.cpp` 这类文件更像手工调试/演示程序，
# 依赖额外环境，不适合作为默认测试执行。
file(GLOB TEST_SOURCES "test*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 提取文件名，不带扩展名
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    message(STATUS "- ${TEST_NAME}")
    # 创建可执行文件
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${TEST_NAME} ${CMAKE_PROJECT_NAME})

    # 添加测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
