function init(n_agents, agent_multiplicity) end
function set_config_parameter(param, value) end
function clear_world_state() end
function update_ship(agent_id, hp, x, y, heading) end
function update_shot(agent_id, lifetime, x, y, heading) end
function update_score(agent_id, score) end

function make_action(agent_id, tick)
    local block = math.floor(tick / 10)
    local turn_flag = (block % 2 == 0) and scubywasm.ACTION_TURN_RIGHT or scubywasm.ACTION_TURN_LEFT
    return scubywasm.ACTION_THRUST | turn_flag | scubywasm.ACTION_FIRE
end
