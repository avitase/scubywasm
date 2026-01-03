#include "engine.c"

#include <math.h>

#include "test/unity.h"

#define DEG2RAD .017453293F

struct Config cfg;
struct Context *ctx;

void setUp(void)
{
    set_default_config(&cfg);
    ctx = create_context(&cfg);
}

void tearDown(void)
{
    free_context(ctx);
}

void test_set_default_config_sets_default_values(void)
{
    set_default_config(&cfg);

    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_SHIP_MAX_TURN_RATE, cfg.ship_max_turn_rate);
    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_SHIP_MAX_VELOCITY, cfg.ship_max_velocity);
    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_SHIP_HIT_RADIUS, cfg.ship_hit_radius);
    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_SHOT_VELOCITY, cfg.shot_velocity);
    TEST_ASSERT_EQUAL_INT32(DEFAULT_SHOT_LIFETIME, cfg.shot_lifetime);
}

void test_add_agent_adds_ships_and_no_shots(void)
{
    const struct Pose init_pose1 = {.x = .5F, .y = 0.F, .heading = 45.F};
    const struct Pose init_pose2 = {.x = 0.F, .y = .5F, .heading = 300.F};

    const uint32_t id1 = add_agent(ctx, init_pose1);
    const uint32_t id2 = add_agent(ctx, init_pose2);
    TEST_ASSERT_GREATER_THAN(0, id1);
    TEST_ASSERT_GREATER_THAN(0, id2);
    TEST_ASSERT_TRUE(id1 != id2);

    TEST_ASSERT_EQUAL_INT32(1, is_alive(ctx, id1));
    TEST_ASSERT_EQUAL_INT32(1, is_alive(ctx, id2));

    struct Pose pose;

    get_ship_pose(ctx, id1, &pose);
    TEST_ASSERT_EQUAL_FLOAT(init_pose1.x, pose.x);
    TEST_ASSERT_EQUAL_FLOAT(init_pose1.y, pose.y);
    TEST_ASSERT_FLOAT_WITHIN(.1F, init_pose1.heading, pose.heading);
    TEST_ASSERT_EQUAL_INT32(0, get_shot_pose(ctx, id1, &pose));
    TEST_ASSERT_EQUAL_INT32(0, get_score(ctx, id1));

    get_ship_pose(ctx, id2, &pose);
    TEST_ASSERT_EQUAL_FLOAT(init_pose2.x, pose.x);
    TEST_ASSERT_EQUAL_FLOAT(init_pose2.y, pose.y);
    TEST_ASSERT_FLOAT_WITHIN(.1F, init_pose2.heading, pose.heading);
    TEST_ASSERT_EQUAL_INT32(0, get_shot_pose(ctx, id2, &pose));
    TEST_ASSERT_EQUAL_INT32(0, get_score(ctx, id2));
}

void test_tick_single_agent_turn_then_move(void)
{
    const struct Pose init_pose = {.x = .25F, .y = .25F, .heading = 90.F};
    const uint32_t id = add_agent(ctx, init_pose);

    TEST_ASSERT_EQUAL_INT32(0, set_action(ctx, id, ACTION_TURN_RIGHT));

    TEST_ASSERT_EQUAL_UINT32(1U, tick(ctx, 1U));
    TEST_ASSERT_EQUAL_INT32(1, is_alive(ctx, id));

    struct Pose p1;
    get_ship_pose(ctx, id, &p1);

    const float heading = init_pose.heading + cfg.ship_max_turn_rate;
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, init_pose.x, p1.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, init_pose.y, p1.y);
    TEST_ASSERT_FLOAT_WITHIN(.1F, heading, p1.heading);

    TEST_ASSERT_EQUAL_INT32(0, set_action(ctx, id, ACTION_THRUST));

    TEST_ASSERT_EQUAL_UINT32(1U, tick(ctx, 1U));
    TEST_ASSERT_EQUAL_INT32(1, is_alive(ctx, id));

    struct Pose p2;
    get_ship_pose(ctx, id, &p2);

    const float x = p1.x + cfg.ship_max_velocity * sinf(p1.heading * DEG2RAD);
    const float y = p1.y + cfg.ship_max_velocity * cosf(p1.heading * DEG2RAD);

    TEST_ASSERT_FLOAT_WITHIN(1e-6F, x, p2.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, y, p2.y);
    TEST_ASSERT_FLOAT_WITHIN(.1F, p1.heading, p2.heading);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_set_default_config_sets_default_values);
    RUN_TEST(test_add_agent_adds_ships_and_no_shots);
    RUN_TEST(test_tick_single_agent_turn_then_move);

    return UNITY_END();
}
