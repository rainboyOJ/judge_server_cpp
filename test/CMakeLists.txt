
# set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")
# set(CMAKE_STATIC_LINKER_FLAGS "-Wl,--copy-dt-needed-entries")

# 只把真正的自动化测试注册进 CTest。
# `main.cpp`、`send_one_test.cpp`、`2.cpp` 这类文件更像手工调试/演示程序，
# 依赖额外环境，不适合作为默认测试执行。
set(TEST_SOURCES
    test_async_submission_flow.cpp
    test_client_sockets_injection.cpp
    test_config_sjudge_security.cpp
    test_connection_registry.cpp
    test_cpp_runner.cpp
    test_integration_tcp_cpp_python.cpp
    test_judge_core.cpp
    test_judge_worker_pool.cpp
    test_protocol_codec.cpp
    test_python_runner.cpp
    test_query_request_handler.cpp
    test_reply_channel.cpp
    test_result_store.cpp
    test_runner_factory.cpp
    test_runner_support_compile.cpp
    test_sjudger_result_mapper.cpp
    test_sjudger.cpp
    test_submission_event_responder.cpp
    test_submission_queue.cpp
    test_submission_request_handler.cpp
    test_submission_service.cpp
    test_submission_types_compile.cpp
    test_tcp_server_eventfd.cpp
)
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 提取文件名，不带扩展名
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    message(STATUS "- ${TEST_NAME}")
    # 创建可执行文件
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${TEST_NAME} boxTest)

    # 添加测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
