set(CMAKE_C_STANDARD 11)

idf_component_register(
        SRCS
        "main.c"

        "config/config.c"

        "ships/ship.c"
        "ships/ship_factory.c"

        "tasks/ship_task.c"

        "queues/ready_queue.c"
        "queues/canal_list.c"

        "schedulers/Scheduler.c"

        "canal/canal.c"

        "flow_policies/flow_policy.c"

        INCLUDE_DIRS
        "."
        "config"
        "common"
        "ships"
        "tasks"
        "queues"
        "schedulers"
        "canal"
        "flow_policies"
)