add_library(thread_pool
        thread_pool.cpp
)

target_link_libraries(thread_pool
        PUBLIC
        queue
)