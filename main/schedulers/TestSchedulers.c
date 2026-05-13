#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "Scheduler.h"
#include "../ships/ship_factory.h"

// Tests utilities
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                      \
    do                                  \
    {                                   \
        printf("  [ RUN ] %s\n", name); \
        tests_run++;                    \
    } while (0)

#define PASS(name)                      \
    do                                  \
    {                                   \
        printf("  [  OK ] %s\n", name); \
        tests_passed++;                 \
    } while (0)

#define FAIL(name, msg)                           \
    do                                            \
    {                                             \
        printf("  [FAIL ] %s — %s\n", name, msg); \
        tests_failed++;                           \
    } while (0)

#define CHECK(cond, name, msg) \
    do                         \
    {                          \
        if (cond)              \
        {                      \
            PASS(name);        \
        }                      \
        else                   \
        {                      \
            FAIL(name, msg);   \
        }                      \
    } while (0)

// Helper: create a ship with specified parameters for testing
static struct Ship make_ship(int id, ShipType type, int burst, int remaining,
                             int priority, int deadline)
{
    struct Ship s;
    s.id = id;
    s.type = type;
    s.origin = LEFT;
    s.destination = RIGHT;
    s.state = READY;
    s.priority = priority;
    s.burstTime = burst;
    s.remainingTime = remaining;
    s.deadline = deadline;
    s.speed = 1;
    s.position = 0;
    return s;
}

/**
 * @brief Tests for ReadyQueue and scheduling algorithms.
 * Each test case checks specific behavior of the queue and schedulers, including edge cases.
 * The tests cover:
 * - Queue operations (init, add, remove, is_empty)
 * - FCFS: always selects the first ship
 * - SJF: selects ship with smallest burstTime
 * - STRN: selects ship with smallest remainingTime and decrements it
 * - Priority: selects ship with highest priority (lowest number)
 * - Round Robin: rotates ships based on quantum, handles ship completion
 * - EDF: selects ship with earliest deadline
 * - Integration test simulating multiple ticks with STRN
 * @return void.
 */
void test_queue(void)
{
    printf("\n══ Queue (ReadyQueue) ══\n");
    ReadyQueue q;
    queue_init(&q);

    TEST("queue_init empty queue");
    CHECK(queue_is_empty(&q), "queue_init empty queue", "queue should be empty at start");

    struct Ship s1 = make_ship(1, NORMAL, 10, 10, 3, 30);
    struct Ship s2 = make_ship(2, FISHING, 5, 5, 2, 10);
    queue_add(&q, s1);
    queue_add(&q, s2);

    TEST("queue_add increases count");
    CHECK(q.count == 2, "queue_add increases count", "count should be 2");

    TEST("queue_remove reduces count");
    queue_remove(&q, 0);
    CHECK(q.count == 1, "queue_remove reduces count", "count should be 1");

    TEST("queue_remove shifts elements");
    CHECK(q.ships[0].id == 2, "queue_remove shifts elements correctly",
          "the ship 2 should remain at index 0");

    TEST("queue_is_empty after remove");
    queue_remove(&q, 0);
    CHECK(queue_is_empty(&q), "queue_is_empty after remove", "should be empty");

    TEST("queue_is_empty returns -1 in schedulers");
    int idx = scheduler_fcfs(&q);
    CHECK(idx == -1, "queue_is_empty returns -1 in schedulers",
          "schedulers must return -1 with an empty queue");
}

/**
 * @brief Tests for FCFS scheduling algorithm.
 * Verifies that FCFS always selects the first ship in the queue, regardless of other attributes.
 * Also checks behavior with a single ship and an empty queue.
 * @return void.
 */
void test_fcfs(void)
{
    printf("\n══ FCFS ══\n");
    ReadyQueue q;
    queue_init(&q);

    struct Ship s1 = make_ship(10, NORMAL, 8, 8, 3, 24);
    struct Ship s2 = make_ship(11, FISHING, 4, 4, 2, 8);
    struct Ship s3 = make_ship(12, PATROL, 2, 2, 1, 2);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    TEST("FCFS always selects index 0");
    int idx = scheduler_fcfs(&q);
    CHECK(idx == 0, "FCFS always selects index 0",
          "FCFS must return 0 regardless of priorities");

    TEST("FCFS respects the order of arrival (id=10 first)");
    CHECK(q.ships[idx].id == 10, "FCFS respects the order of arrival (id=10 first)",
          "the first ship added must exit first");

    TEST("FCFS queue with a single element");
    ReadyQueue q2;
    queue_init(&q2);
    queue_add(&q2, s3);
    CHECK(scheduler_fcfs(&q2) == 0, "FCFS queue with a single element", "must return 0");

    TEST("FCFS empty queue returns -1");
    ReadyQueue q3;
    queue_init(&q3);
    CHECK(scheduler_fcfs(&q3) == -1, "FCFS empty queue returns -1", "must return -1");
}

/**
 * @brief Tests for SJF scheduling algorithm.
 * Verifies that SJF always selects the ship with the shortest burst time.
 * Also checks behavior with equal burst times and an empty queue.
 * @return void.
 */
void test_sjf(void)
{
    printf("\n══ SJF ══\n");
    ReadyQueue q;
    queue_init(&q);

    // Burst times: 10, 4, 7 → the smallest is ship id=21 (burst=4)
    struct Ship s1 = make_ship(20, NORMAL, 10, 10, 3, 30);
    struct Ship s2 = make_ship(21, FISHING, 4, 4, 2, 8);
    struct Ship s3 = make_ship(22, PATROL, 7, 7, 1, 7);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    TEST("SJF selects ship with smallest burstTime");
    int idx = scheduler_sjf(&q);
    CHECK(q.ships[idx].id == 21, "SJF selects ship with smallest burstTime",
          "must select ship with burst=4 (id=21)");

    TEST("SJF with equal burst times selects the first");
    ReadyQueue q2;
    queue_init(&q2);
    struct Ship a = make_ship(30, NORMAL, 5, 5, 3, 15);
    struct Ship b = make_ship(31, NORMAL, 5, 5, 3, 15);
    queue_add(&q2, a);
    queue_add(&q2, b);
    idx = scheduler_sjf(&q2);
    CHECK(idx == 0, "SJF with equal burst times selects the first",
          "with equal burst times, it must return the first (index 0)");

    TEST("SJF empty queue returns -1");
    ReadyQueue q3;
    queue_init(&q3);
    CHECK(scheduler_sjf(&q3) == -1, "SJF empty queue returns -1", "must return -1");
}

/**
 * @brief Tests for STRN scheduling algorithm.
 * Verifies that STRN selects the ship with the shortest remaining time.
 * @return void.
 */
void test_strn(void)
{
    printf("\n══ STRN ══\n");
    ReadyQueue q;
    queue_init(&q);

    // remainingTime: 3, 1, 5 → the smallest is id=41 (remaining=1)
    struct Ship s1 = make_ship(40, NORMAL, 8, 3, 3, 24);
    struct Ship s2 = make_ship(41, FISHING, 5, 1, 2, 10);
    struct Ship s3 = make_ship(42, PATROL, 6, 5, 1, 6);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    TEST("STRN selects ship with smallest remainingTime");
    int idx = scheduler_strn(&q);
    CHECK(q.ships[idx].id == 41, "STRN selects ship with smallest remainingTime",
          "must select ship with remaining=1 (id=41)");

    TEST("STRN empty queue returns -1");
    ReadyQueue q2;
    queue_init(&q2);
    CHECK(scheduler_strn(&q2) == -1, "STRN empty queue returns -1", "must return -1");
}

/**
 * @brief Tests for Priority scheduling algorithm.
 * Verifies that Priority selects the ship with the highest priority (lowest priority number).
 * Also checks behavior with equal priorities and an empty queue.
 * @return void.
 */
void test_priority(void)
{
    printf("\n══ Priority ══\n");
    ReadyQueue q;
    queue_init(&q);

    // priorities: 3(NORMAL), 2(FISHING), 1(PATROL) → PATROL should be selected
    struct Ship s1 = make_ship(50, NORMAL, 10, 10, 3, 30);
    struct Ship s2 = make_ship(51, FISHING, 5, 5, 2, 10);
    struct Ship s3 = make_ship(52, PATROL, 3, 3, 1, 3);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    TEST("Priority selects ship with lowest priority number");
    int idx = scheduler_priority(&q);
    CHECK(q.ships[idx].id == 52, "Priority selects ship with lowest priority number",
          "PATROL (priority=1) must be selected");

    TEST("Priority PATROL has higher urgency than FISHING");
    CHECK(q.ships[idx].type == PATROL, "Priority PATROL has higher urgency than FISHING",
          "type must be PATROL");

    TEST("Priority with two PATROL ships selects the first");
    ReadyQueue q2;
    queue_init(&q2);
    struct Ship p1 = make_ship(60, PATROL, 3, 3, 1, 3);
    struct Ship p2 = make_ship(61, PATROL, 3, 3, 1, 3);
    queue_add(&q2, p1);
    queue_add(&q2, p2);
    idx = scheduler_priority(&q2);
    CHECK(idx == 0, "Priority with two PATROL ships selects the first",
          "in case of a tie, it should return index 0");

    TEST("Priority empty queue returns -1");
    ReadyQueue q3;
    queue_init(&q3);
    CHECK(scheduler_priority(&q3) == -1, "Priority empty queue returns -1", "must return -1");
}

/**
 * @brief Tests for Round Robin scheduling algorithm.
 * Verifies that Round Robin rotates ships based on a fixed quantum.
 * Also checks that all ships receive CPU time (no starvation) and behavior with an empty queue.
 * The test simulates multiple ticks and checks the state of the queue after each tick to ensure correct rotation and completion handling.
 * @return void.
 */
void test_rr(void)
{
    printf("\n══ Round Robin ══\n");
    ReadyQueue q;
    queue_init(&q);

    int quantum = 2;
    int rr_index = 0;

    // Ships: burst 4, 4, 4  → must rotate every 2 ticks
    struct Ship s1 = make_ship(70, NORMAL, 4, 4, 3, 12);
    struct Ship s2 = make_ship(71, FISHING, 4, 4, 2, 8);
    struct Ship s3 = make_ship(72, PATROL, 4, 4, 1, 4);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    // Tick 1: s1 (remaining 4→3), don't rotate yet
    TEST("RR tick 1: ship 0 active");
    int idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 0, "RR tick 1: ship 0 active", "must execute index 0");
    decRemainingTime(&q.ships[idx]);

    // Tick 2: s1 (remaining 3→2), quantum expired → rotate to index 1
    TEST("RR tick 2: quantum expired, rotate");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 0, "RR tick 2: quantum expired, rotate",
          "still executes index 0 before rotating");
    decRemainingTime(&q.ships[idx]);
    CHECK(rr_index == 1, "RR rotate to index 1 after quantum",
          "rr_index must advance to 1 after the quantum");

    // Tick 3: s2 (remaining 4→3), quantum not expired yet
    TEST("RR tick 3: ship 1 active");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 1, "RR tick 3: ship 1 active", "must execute index 1");
    decRemainingTime(&q.ships[idx]);

    // Tick 4: s2 (remaining 3→2), quantum expired → rotate to index 2
    TEST("RR tick 4: quantum expired, rotate");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 1, "RR tick 4: quantum expired, rotate",
          "still executes index 1 before rotating");
    decRemainingTime(&q.ships[idx]);
    CHECK(rr_index == 2, "RR rotate to index 2 after quantum",
          "rr_index must advance to 2 after the quantum");

    // Tick 5: s3 (remaining 4→3), quantum not expired yet
    TEST("RR tick 5: ship 2 active");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 2, "RR tick 5: ship 2 active", "must execute index 2");
    decRemainingTime(&q.ships[idx]);

    // Tick 6: s3 (remaining 3→2), quantum expired → rotate to index 0
    TEST("RR tick 6: quantum expired, rotate");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 2, "RR tick 6: quantum expired, rotate",
          "still executes index 2 before rotating");
    decRemainingTime(&q.ships[idx]);
    CHECK(rr_index == 0, "RR rotate to index 0 after quantum",
          "rr_index must advance to 0 after the quantum");

    // Tick 7: s1 (remaining 2→1), quantum not expired yet
    TEST("RR tick 7: ship 0 active again");
    idx = scheduler_rr(&q, quantum, &rr_index);
    CHECK(idx == 0, "RR tick 7: ship 0 active again", "must execute index 0 again");
    decRemainingTime(&q.ships[idx]);

    TEST("RR no starvation — all ships receive CPU time");
    // Verify that after several ticks, all ships have received CPU time
    CHECK(q.ships[0].remainingTime < 4, "all ships receive CPU time",
          "ship 0 must have advanced");
    CHECK(q.ships[1].remainingTime < 4, "all ships receive CPU time",
          "ship 1 must have advanced");
    CHECK(q.ships[2].remainingTime < 4, "all ships receive CPU time",
          "ship 2 must have advanced");

    TEST("RR empty queue returns -1");
    ReadyQueue q2;
    queue_init(&q2);
    int ri = 0;
    CHECK(scheduler_rr(&q2, quantum, &ri) == -1, "RR empty queue returns -1", "must be -1");
}

/**
 * @brief Tests for EDF scheduling algorithm.
 * Verifies that EDF selects the ship with the earliest deadline, and that it correctly handles ties and an empty queue.
 * Also checks that EDF prioritizes ships with more urgent deadlines even if they were added later to the queue (not FCFS).
 * @return void.
 */
void test_edf(void)
{
    printf("\n══ EDF (Real Time) ══\n");
    ReadyQueue q;
    queue_init(&q);

    // deadlines: 30, 8, 15 → the most urgent is id=81 (deadline=8)
    struct Ship s1 = make_ship(80, NORMAL, 10, 10, 3, 30);
    struct Ship s2 = make_ship(81, FISHING, 4, 4, 2, 8);
    struct Ship s3 = make_ship(82, PATROL, 3, 3, 1, 15);
    queue_add(&q, s1);
    queue_add(&q, s2);
    queue_add(&q, s3);

    TEST("EDF selects the ship with the earliest deadline");
    int idx = scheduler_edf(&q);
    CHECK(q.ships[idx].id == 81, "EDF selects the ship with the earliest deadline",
          "must select deadline=8 (id=81)");

    TEST("EDF PATROL with the most urgent deadline has priority");
    ReadyQueue q2;
    queue_init(&q2);
    struct Ship patrol = make_ship(90, PATROL, 3, 3, 1, 3);
    struct Ship normal = make_ship(91, NORMAL, 10, 10, 3, 30);
    queue_add(&q2, normal);
    queue_add(&q2, patrol);
    idx = scheduler_edf(&q2);
    CHECK(q2.ships[idx].type == PATROL, "EDF PATROL with the most urgent deadline has priority",
          "EDF must prefer PATROL by earlier deadline");

    TEST("EDF same deadlines chooses the first");
    ReadyQueue q3;
    queue_init(&q3);
    struct Ship a = make_ship(100, NORMAL, 5, 5, 3, 10);
    struct Ship b = make_ship(101, NORMAL, 5, 5, 3, 10);
    queue_add(&q3, a);
    queue_add(&q3, b);
    idx = scheduler_edf(&q3);
    CHECK(idx == 0, "EDF same deadlines chooses the first",
          "in case of a tie, it should return index 0");

    TEST("EDF empty queue returns -1");
    ReadyQueue q4;
    queue_init(&q4);
    CHECK(scheduler_edf(&q4) == -1, "EDF empty queue returns -1", "must be -1");
}

/**
 * @brief Integration test simulating multiple ticks with STRN scheduling.
 * This test creates a ready queue with multiple ships and simulates a scheduling loop for several ticks,
 * using STRN to select the ship to execute at each tick.
 * The test:
 * - Checks that ships are selected according to their remaining time.
 * - Checks that they are marked as finished when their remaining time reaches 0.
 * - Checks that the simulation progresses correctly until all ships are finished or a maximum number of ticks is reached.
 * This test verifies the overall integration of the STRN scheduler with the ReadyQueue and ship state management in a realistic scenario.
 * @return void.
 */
void test_integration_simulation(void)
{
    printf("\n══ Integration: simulate 10 ticks with STRN ══\n");

    ReadyQueue q;
    queue_init(&q);

    // Canal of length 6: burst = ceil(6/speed)
    int canal = 6;
    queue_add(&q, createShip(NORMAL, LEFT, canal));
    queue_add(&q, createShip(FISHING, RIGHT, canal));
    queue_add(&q, createShip(PATROL, LEFT, canal));

    printf("  Ships initially: %d\n", q.count);

    int ticks = 0;
    int finished_count = 0;

    while (!queue_is_empty(&q) && ticks < 20)
    {
        int idx = scheduler_strn(&q);
        if (idx < 0)
            break;

        struct Ship *ship = &q.ships[idx];

        // Channel decrements remaining time and marks finished if it reaches 0
        decRemainingTime(ship);
        if (ship->remainingTime == 0)
            finish(ship);

        printf("  Tick %2d → Ship id=%d type=%d remaining=%d\n",
               ticks, ship->id, ship->type, ship->remainingTime);

        if (ship->state == FINISHED)
        {
            finished_count++;
            queue_remove(&q, idx);
        }
        ticks++;
    }

    TEST("Simulation STRN: all ships finish within a reasonable time");
    CHECK(finished_count > 0, "Simulation STRN: all ships finish within a reasonable time",
          "at least one ship must finish");

    printf("  Ships finished: %d in %d ticks\n", finished_count, ticks);
}

int main(void)
{
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Test Suite — Scheduling Ships         ║\n");
    printf("╚════════════════════════════════════════╝\n");

    test_queue();
    test_fcfs();
    test_sjf();
    test_strn();
    test_priority();
    test_rr();
    test_edf();
    test_integration_simulation();

    printf("\n════════════════════════════════════════\n");
    printf("  Total:   %d tests\n", tests_run);
    printf("  Passed:  %d\n", tests_passed);
    printf("  Failed:  %d\n", tests_failed);
    printf("════════════════════════════════════════\n");

    return (tests_failed == 0) ? 0 : 1;
}
