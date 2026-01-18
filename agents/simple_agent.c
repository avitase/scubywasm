#include "scubywasm_agent.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct Context *init_agent(uint32_t /*n_agents*/,
                           uint32_t /*agent_multiplicity*/)
{
    return NULL;
}

void free_context(struct Context * /*ctx*/)
{
}

void set_config_parameter(struct Context * /*ctx*/,
                          enum ConfigParameter /*p*/,
                          float /*value*/)
{
}

void clear_world_state(struct Context * /*ctx*/)
{
}

void update_ship(struct Context * /*ctx*/,
                 uint32_t /*agent_id*/,
                 int32_t /*hp*/,
                 float /*x*/,
                 float /*y*/,
                 float /*heading*/)
{
}

void update_shot(struct Context * /*ctx*/,
                 uint32_t /*agent_id*/,
                 int32_t /*lifetime*/,
                 float /*x*/,
                 float /*y*/,
                 float /*heading*/)
{
}

void update_score(struct Context * /*ctx*/,
                  uint32_t /*agent_id*/,
                  int32_t /*score*/)
{
}

uint32_t
make_action(struct Context * /*ctx*/, uint32_t /*agent_id*/, uint32_t /*tick*/)
{
    const uint32_t thrust = 0x01;
    const uint32_t turn_left = 0x02;
    const uint32_t fire = 0x08;

    return thrust | turn_left | fire;
}

#ifdef __cplusplus
}
#endif
