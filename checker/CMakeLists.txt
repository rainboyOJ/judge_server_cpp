cmake_minimum_required(VERSION 3.10)
project(checker)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

set(BASE_CMAKE_C_FLAGS "-Wall -Werror -O2")
set(CMAKE_C_FLAGS "${BASE_CMAKE_C_FLAGS}")


file(GLOB SOURCE "src/*.cpp")
# message(${CMAKE_CURRENT_SOURCE_DIR})

foreach(f ${SOURCE})
    # message(${f})
    string(REPLACE ".cpp" "" testname ${f})
    string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/src/ "" name ${testname})
    # message(${name})
    add_executable(${name} ${f})
    install(TARGETS ${name}  RUNTIME DESTINATION /judge/checker)
endforeach(f)

#install(DIRECOTRY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/  RUNTIME DESTINATION /judge/checker)
