set(target thread_pool_test)

add_executable(${target} thread_pool.cpp)

target_link_libraries(${target}
        PRIVATE
        thread_pool
        GTest::GTest
        GTest::Main
)

add_test(NAME ${target} COMMAND ${target})