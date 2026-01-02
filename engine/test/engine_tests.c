#include "engine.c"

#include "test/unity.h"

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

void test_add_agent_adds_ship_and_no_shot(void)
{
    const struct Pose init_pose = {.x = 0.F, .y = 0.F, .heading = 45.F};
    const uint32_t id = add_agent(ctx, init_pose);
    TEST_ASSERT_TRUE(id > 0);

    TEST_ASSERT_EQUAL_INT32(1, is_alive(ctx, id));

    struct Pose pose;
    get_ship_pose(ctx, id, &pose);
    TEST_ASSERT_EQUAL_FLOAT(init_pose.x, pose.x);
    TEST_ASSERT_EQUAL_FLOAT(init_pose.y, pose.y);
    TEST_ASSERT_FLOAT_WITHIN(.1F, init_pose.heading, pose.heading);

    TEST_ASSERT_EQUAL_INT32(0, get_shot_pose(ctx, id, &pose));

    TEST_ASSERT_EQUAL_INT32(0, get_score(ctx, id));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_set_default_config_sets_default_values);
    RUN_TEST(test_add_agent_adds_ship_and_no_shot);

    return UNITY_END();
}
