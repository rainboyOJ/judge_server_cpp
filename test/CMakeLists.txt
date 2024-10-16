file(GLOB TEST_SOURCES "*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 提取文件名，不带扩展名
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    message(STATUS "- ${TEST_NAME}")
    # 创建可执行文件
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    # 添加测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
