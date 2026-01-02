#include "engine.h"

#include <stddef.h>
#include <stdint.h>

#if !FREESTANDING
#    include <stdlib.h>
#endif

#ifndef MAX_AGENTS
#    define MAX_AGENTS 128
#endif

#ifndef DEFAULT_SHIP_MAX_TURN_RATE
#    define DEFAULT_SHIP_MAX_TURN_RATE 10.F
#endif

#ifndef DEFAULT_SHIP_MAX_VELOCITY
#    define DEFAULT_SHIP_MAX_VELOCITY .001F
#endif

#ifndef DEFAULT_SHIP_HIT_RADIUS
#    define DEFAULT_SHIP_HIT_RADIUS .01F
#endif

#ifndef DEFAULT_SHOT_VELOCITY
#    define DEFAULT_SHOT_VELOCITY .004F
#endif

#ifndef DEFAULT_SHOT_LIFETIME
#    define DEFAULT_SHOT_LIFETIME 500
#endif

#define AGENT_ID_XOR_MASK 0xABCD
static_assert(
    AGENT_ID_XOR_MASK <= 0xFFFF,
    "AGENT_ID_XOR_MASK must be <= 0xFFFF so agent IDs fit in 16 bits");
static_assert(
    AGENT_ID_XOR_MASK >= MAX_AGENTS,
    "AGENT_ID_XOR_MASK must be >= MAX_AGENTS so agent ID 0 is unreachable.");

#ifdef __cplusplus
extern "C"
{
#endif

struct Vec2D
{
    float x;
    float y;
};

struct Kinematics
{
    struct Vec2D pos;
    struct Vec2D heading;
    float v;
};

struct Ship
{
    struct Kinematics kinematics;
    int32_t is_alive;
};

struct Shot
{
    struct Kinematics kinematics;
    int32_t lifetime;
};

struct Context
{
#if FREESTANDING
    int32_t in_use;
#endif
    struct Config cfg;
    uint32_t n_agents;
    int32_t scores[MAX_AGENTS];
    struct Ship ships[MAX_AGENTS];
    struct Shot shots[MAX_AGENTS];
};

[[nodiscard]] static float
wrap(const float x, const float x_min, const float x_max)
{
    const float period = x_max - x_min;
    return x + (period * ((float)(x < x_min) - ((float)(x >= x_max))));
}

[[nodiscard]] static float
clamp(const float x, const float x_min, const float x_max)
{
    const float dx =
        ((x_min - x) * (float)(x < x_min)) + ((x_max - x) * (float)(x > x_max));
    return x + dx;
}

[[nodiscard]] static float approx_sin(float x)
{
    x -= (x >= 360.F) * 360.F;

    static const float y[64] = {
        0.0F,         0.09956785F,  0.19814614F,  0.29475517F,  0.38843480F,
        0.47825398F,  0.56332006F,  0.64278761F,  0.71586685F,  0.78183148F,
        0.84002592F,  0.88987181F,  0.93087375F,  0.96262425F,  0.98480775F,
        0.99720380F,  0.99968918F,  0.99223921F,  0.97492791F,  0.94792735F,
        0.91150585F,  0.86602540F,  0.81193801F,  0.74978120F,  0.68017274F,
        0.60380441F,  0.52143520F,  0.43388374F,  0.34202014F,  0.24675740F,
        0.14904227F,  0.04984589F,  -0.04984589F, -0.14904227F, -0.24675740F,
        -0.34202014F, -0.43388374F, -0.52143520F, -0.60380441F, -0.68017274F,
        -0.74978120F, -0.81193801F, -0.86602540F, -0.91150585F, -0.94792735F,
        -0.97492791F, -0.99223921F, -0.99968918F, -0.99720380F, -0.98480775F,
        -0.96262425F, -0.93087375F, -0.88987181F, -0.84002592F, -0.78183148F,
        -0.71586685F, -0.64278761F, -0.56332006F, -0.47825398F, -0.38843480F,
        -0.29475517F, -0.19814614F, -0.09956785F, 0.0F,
    };
    const float n = (float)(sizeof(y) / sizeof(y[0])) - 1.0F;

    const uint32_t idx = (uint32_t)((x / 360.F) * n);
    const float dx = 360.F / n;
    const float x0 = (float)idx * dx;
    const float t = (x - x0) / dx;

    return ((1.F - t) * y[idx]) + (t * y[idx + 1]);
}

[[nodiscard]] static float approx_cos(const float x)
{
    return approx_sin(x + 90);
}

[[nodiscard]] static float approx_heading_angle(const struct Vec2D heading)
{
    const float u = heading.x;
    const float v = heading.y;

    const float abs_u = (u < 0.F) ? -u : u;
    const float abs_v = (v < 0.F) ? -v : v;
    const float r = (abs_v - abs_u) / (abs_v + abs_u);

    const float A = 11.2471615F;
    const float B = 56.2472667F;
    const float sign_v = (v < 0.F) ? -1.F : 1.F;
    const float angle = 90.F + sign_v * (((A * r * r - B) * r) - 45.F);

    return wrap((u < 0.F) ? (360.F - angle) : angle, 0.F, 360.F);
}

#if FREESTANDING
[[nodiscard]] struct Config *get_config_buffer(void)
{
    static struct Config cfg;
    return &cfg;
}
#endif

void set_default_config(struct Config *cfg)
{
    *cfg = (struct Config){
        .ship_max_turn_rate = DEFAULT_SHIP_MAX_TURN_RATE,
        .ship_max_velocity = DEFAULT_SHIP_MAX_VELOCITY,
        .ship_hit_radius = DEFAULT_SHIP_HIT_RADIUS,
        .shot_velocity = DEFAULT_SHOT_VELOCITY,
        .shot_lifetime = DEFAULT_SHOT_LIFETIME,
    };
}

struct Context *create_context(const struct Config *cfg)
{
    if (cfg == NULL)
    {
        return NULL;
    }

#if FREESTANDING
    static struct Context ctx = {.in_use = 0, .n_agents = 0U};
    if (ctx.in_use == 0)
    {
        ctx.in_use = 1;
        ctx.cfg = *cfg;
        return &ctx;
    }
#else
    struct Context *ctx = malloc(sizeof(struct Context));
    if (ctx)
    {
        ctx->cfg = *cfg;
        ctx->n_agents = 0U;
        return ctx;
    }
#endif

    return NULL;
}

void free_context(struct Context *ctx)
{
    if (ctx)
    {
#if FREESTANDING
        ctx->in_use = 0;
        ctx->n_agents = 0;
#else
        free(ctx);
#endif
    }
}

[[nodiscard]] static uint32_t agent_id_to_index(uint32_t agent_id)
{
    return agent_id ^ AGENT_ID_XOR_MASK;
}

[[nodiscard]] static uint32_t agent_index_to_id(uint32_t agent_index)
{
    return agent_id_to_index(agent_index);
}

uint32_t add_agent(struct Context *ctx, const struct Pose pose)
{
    if (ctx == NULL)
    {
        return 0U;
    }

    const uint32_t n = ctx->n_agents;
    if (n >= MAX_AGENTS)
    {
        return 0U;
    }

    ctx->n_agents = n + 1;
    ctx->scores[n] = 0;

    ctx->ships[n] =
        (struct Ship){.kinematics = {.pos = {.x = pose.x, .y = pose.y},
                                     .heading = {.x = approx_sin(pose.heading),
                                                 .y = approx_cos(pose.heading)},
                                     .v = 0.F},
                      .is_alive = 1};

    ctx->shots[n].lifetime = 0;

    return agent_index_to_id(n);
}

int32_t set_action(struct Context *ctx, const struct Action action)
{
    const uint32_t idx = agent_id_to_index(action.agent_id);
    if (idx >= MAX_AGENTS)
    {
        return -1;
    }

    if (ctx->ships[idx].is_alive != 1)
    {
        return -2;
    }

    const enum ActionType t = action.type;
    const uint32_t thrust = ((t & ACTION_THRUST) == ACTION_THRUST);
    const uint32_t turn_left = ((t & ACTION_TURN_LEFT) == ACTION_TURN_LEFT);
    const uint32_t turn_right = ((t & ACTION_TURN_RIGHT) == ACTION_TURN_RIGHT);
    const uint32_t fire = ((t & ACTION_FIRE) == ACTION_FIRE);

    const struct Config *cfg = &(ctx->cfg);
    struct Kinematics *ship = &(ctx->ships[idx].kinematics);

    // action: thrust
    ship->v = thrust ? cfg->ship_max_velocity : 0.F;

    // action: turn left or right
    if (turn_left ^ turn_right)
    {
        const float angle = approx_heading_angle(ship->heading)
            + ((turn_left ? -1.F : 1.F) * cfg->ship_max_turn_rate);
        ship->heading.x = approx_sin(angle);
        ship->heading.y = approx_cos(angle);
    }

    // action: fire
    if (fire)
    {
        const float r = cfg->ship_hit_radius * 1.001F;
        ctx->shots[idx] = (struct Shot){
            .kinematics =
                {
                    .pos = {.x = ship->pos.x + r * ship->heading.x,
                            .y = ship->pos.y + r * ship->heading.y},
                    .heading = ship->heading,
                    .v = cfg->shot_velocity,
                },
            .lifetime = cfg->shot_lifetime,
        };
    }

    return 0;
}

[[nodiscard]] static struct Vec2D propagate(const struct Kinematics kinematics)
{
    const float v = kinematics.v;
    return (struct Vec2D){
        .x = wrap(kinematics.pos.x + v * kinematics.heading.x, 0.F, 1.F),
        .y = wrap(kinematics.pos.y + v * kinematics.heading.y, 0.F, 1.F),
    };
}

[[nodiscard]] static float doca2(const struct Kinematics obj1,
                                 const struct Kinematics obj2)
{
    const struct Vec2D v = {
        .x = obj1.v * obj1.heading.x - obj2.v * obj2.heading.x,
        .y = obj1.v * obj1.heading.y - obj2.v * obj2.heading.y};
    const float v2 = v.x * v.x + v.y * v.y;

    const struct Vec2D p = {.x = obj1.pos.x - obj2.pos.x,
                            .y = obj1.pos.y - obj2.pos.y};

    float min_d2 = 10.F; // an unreasonably large distance on the unit torus
    for (int32_t dx = -1; dx <= 1; dx++)
    {
        for (int32_t dy = -1; dy <= 1; dy++)
        {
            const struct Vec2D q = {.x = p.x - (float)dx, .y = p.y - (float)dy};
            const float qv = (q.x * v.x) + (q.y * v.y);
            const float t = clamp((v2 < 1e-30F) ? 0.F : -qv / v2, 0.F, 1.F);

            const struct Vec2D d = {.x = q.x + v.x * t, .y = q.y + v.y * t};
            const float d2 = d.x * d.x + d.y * d.y;

            min_d2 = (d2 < min_d2) ? d2 : min_d2;
        }
    }

    return min_d2;
}

[[nodiscard]] static int32_t sweep_test(const struct Kinematics obj1,
                                        const struct Kinematics obj2,
                                        const float threshold)
{
    return doca2(obj1, obj2) < threshold;
}

static void tick_once(struct Context *ctx)
{
    const uint32_t n = ctx->n_agents;

    // check for collisions ships <> shots
    const float r = ctx->cfg.ship_hit_radius;
    const float r2 = r * r;
    for (uint32_t i = 0; i < n; i++)
    {
        struct Shot *shot = ctx->shots + i;
        for (uint32_t j = 0; j < n; j++)
        {
            struct Ship *ship = ctx->ships + j;
            if (shot->lifetime != 0 && ship->is_alive != 0
                && sweep_test(shot->kinematics, ship->kinematics, r2))
            {
                shot->lifetime = -1;
                ship->is_alive = -1;

                ctx->scores[i] += 2;
                ctx->scores[j] -= 1;
            }
        }
    }

    // check for collisions ships <> ships
    for (uint32_t i = 0; i < n; i++)
    {
        struct Ship *ship1 = ctx->ships + i;
        for (uint32_t j = i + 1; j < n; j++)
        {
            struct Ship *ship2 = ctx->ships + j;
            if (ship1->is_alive * ship2->is_alive != 0
                && sweep_test(ship1->kinematics, ship2->kinematics, 4.F * r2))
            {
                ship1->is_alive = -1;
                ship2->is_alive = -1;

                ctx->scores[i] -= 1;
                ctx->scores[j] -= 1;
            }
        }
    }

    // propagate ships & shots
    for (uint32_t i = 0; i < n; i++)
    {
        struct Ship *ship = ctx->ships + i;
        ship->kinematics.pos = propagate(ship->kinematics);
        ship->is_alive = (ship->is_alive == 1);

        struct Shot *shot = ctx->shots + i;
        shot->kinematics.pos = propagate(shot->kinematics);
        shot->lifetime = (shot->lifetime > 0) ? shot->lifetime - 1 : 0;
    }
}

uint32_t tick(struct Context *ctx, const uint32_t n_times)
{
    if (ctx == NULL)
    {
        return 0U;
    }

    for (uint32_t i = 0; i < n_times; i++)
    {
        tick_once(ctx);
    }

    const uint32_t n_agents = ctx->n_agents;

    uint32_t n_alive = 0;
    for (uint32_t i = 0; i < n_agents; i++)
    {
        n_alive += (uint32_t)(ctx->ships[i].is_alive == 1);
    }

    return n_alive;
}

#if FREESTANDING
struct Pose *get_pose_buffer(void)
{
    static struct Pose pose;
    return &pose;
}
#endif

[[nodiscard]] static struct Pose approx_pose(const struct Kinematics kinematics)
{
    return (struct Pose){.x = kinematics.pos.x,
                         .y = kinematics.pos.y,
                         .heading = approx_heading_angle(kinematics.heading)};
}

void get_ship_pose(const struct Context *ctx,
                   uint32_t agent_id,
                   struct Pose *pose)
{
    const uint32_t idx = agent_id_to_index(agent_id);
    if (idx < MAX_AGENTS && ctx && pose)
    {
        *pose = approx_pose(ctx->ships[idx].kinematics);
    }
}

int32_t get_shot_pose(const struct Context *ctx,
                      const uint32_t agent_id,
                      struct Pose *pose)
{
    const uint32_t idx = agent_id_to_index(agent_id);
    if (idx < MAX_AGENTS && ctx)
    {
        const struct Shot *shot = ctx->shots + idx;
        *pose = approx_pose(shot->kinematics);

        return shot->lifetime;
    }

    return 0U;
}

int32_t is_alive(const struct Context *ctx, const uint32_t agent_id)
{
    const uint32_t idx = agent_id_to_index(agent_id);
    return (idx < MAX_AGENTS && ctx) ? ctx->ships[idx].is_alive : 0;
}

int32_t get_score(const struct Context *ctx, const uint32_t agent_id)
{
    const uint32_t idx = agent_id_to_index(agent_id);
    return (idx < MAX_AGENTS && ctx) ? ctx->scores[idx] : 0;
}

#ifdef __cplusplus
}
#endif
