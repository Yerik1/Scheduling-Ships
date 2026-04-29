set(CMAKE_C_STANDARD 11)

idf_component_register(
        SRCS
        "main.c"
        "ships/ship.c"
        "ships/ship_factory.c"
        "tasks/ship_task.c"
        INCLUDE_DIRS
        "."
        "common"
        "ships"
        "tasks"
)