#ifndef SIMPLE_AGENT_H
#define SIMPLE_AGENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum ConfigParameter : unsigned int
{
    CFG_SHIP_MAX_TURN_RATE = 0,
    CFG_SHIP_MAX_VELOCITY = 1,
    CFG_SHIP_HIT_RADIUS = 2,
    CFG_SHOT_VELOCITY = 3,
    CFG_SHOT_LIFETIME = 4,
};

struct Context;

struct Context *init_agent(uint32_t n_agents);

void free_context(struct Context *ctx);

void set_config_parameter(Context *ctx, enum ConfigParameter, float value);

void clear_world_state(struct Context *ctx);

void update_ship(struct Context *ctx,
                 uint32_t agent_id,
                 int32_t hp,
                 float x,
                 float y,
                 float heading);

void update_shot(struct Context *ctx,
                 uint32_t agent_id,
                 int32_t lifetime,
                 float x,
                 float y,
                 float heading);

void update_score(struct Context *ctx, uint32_t agent_id, int32_t score);

uint32_t make_action(struct Context *ctx, uint32_t agent_id, uint32_t tick);

#ifdef __cplusplus
}
#endif
#endif // SIMPLE_AGENT_H
