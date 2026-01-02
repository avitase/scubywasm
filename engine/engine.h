#ifndef ENGINE_H
#define ENGINE_H

#ifndef FREESTANDING
#    define FREESTANDING 1
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum ActionFlags : unsigned int
{
    ACTION_NONE = 0x00,
    ACTION_THRUST = 0x01,
    ACTION_TURN_LEFT = 0x02,
    ACTION_TURN_RIGHT = 0x04,
    ACTION_FIRE = 0x08,
};

struct Config
{
    float ship_max_turn_rate;
    float ship_max_velocity;
    float ship_hit_radius;
    float shot_velocity;
    int32_t shot_lifetime;
};

struct Pose
{
    float x;
    float y;
    float heading;
};

struct Context;

#if FREESTANDING
[[nodiscard]] struct Config *get_config_buffer(void);
#endif

void set_default_config(struct Config *cfg);

[[nodiscard]] struct Context *create_context(const struct Config *cfg);

void free_context(struct Context *ctx);

#if FREESTANDING
[[nodiscard]] struct Pose *get_pose_buffer(void);
#endif

uint32_t add_agent(struct Context *ctx, struct Pose);

int32_t set_action(struct Context *ctx, uint32_t agent_id, enum ActionFlags);

uint32_t tick(struct Context *ctx, uint32_t n_times);

void get_ship_pose(const struct Context *ctx,
                   uint32_t agent_id,
                   struct Pose *pose);

int32_t get_shot_pose(const struct Context *ctx,
                      uint32_t agent_id,
                      struct Pose *pose);

[[nodiscard]] int32_t is_alive(const struct Context *ctx,
                               uint32_t agent_id);

[[nodiscard]] int32_t get_score(const struct Context *ctx,
                                uint32_t agent_id);

#ifdef __cplusplus
}
#endif
#endif // ENGINE_H
