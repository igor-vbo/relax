cmake_minimum_required(VERSION 3.6)
project(RelaxTest)


set(CMAKE_CXX_STANDARD 20)
if(WIN32)
    # bug in msvc
    set(CMAKE_CXX_STANDARD 23)
endif()

if(LINUX)
    set(CMAKE_CXX_COMPILER clang)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -Wall -Wextra")
endif()

if(WIN32)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_compile_options("/volatile:ms")
endif()


add_compile_options("-lgtest")
add_compile_options("-pthread")

set(HEADER_FILES
    ./src/test/test.h
    ./src/map/map.h
    ./src/map/testgen.h
    ./src/map/intrusive_map.h
    ./src/queue/intrusive_queue.h
    ./src/queue/queue.h
    ./src/types.h
    ./src/sync/atomic.h
    ./src/sync/event.h
    ./src/common.h
    ./src/utils/utils.h
)

set(SOURCE_FILES
    ./src/main.cpp
    ./src/map/ut.cpp
    ./src/map/bench.cpp
    ./src/queue/ut.cpp
    ./src/queue/verification.cpp
    ./src/queue/bench.cpp
)

add_executable(relaxtest ${HEADER_FILES} ${SOURCE_FILES} )
target_include_directories(relaxtest PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(relaxtest gtest gtest_main)

if(WIN32)
    include(GoogleTest)
    gtest_discover_tests(relaxtest)
endif()
