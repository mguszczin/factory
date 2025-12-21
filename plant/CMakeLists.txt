add_library(plant
    solution.c
    src/factory.c
    src/task_info.c
    src/task_list.c
    src/worker_info.c
    src/worker_list.c
)

target_include_directories(plant
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/headers
)

target_link_libraries(plant PRIVATE err)
