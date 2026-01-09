from .wasmmodule import WASMModule


class Agent:
    def __init__(self, wasm, *, store, n_agents_total, engine_cfg):
        self._module = WASMModule(wasm, store=store)
        self._ctx = self._module.init_agent(n_agents_total)

        for i, value in enumerate(
            [
                engine_cfg.ship_max_turn_rate,
                engine_cfg.ship_max_velocity,
                engine_cfg.ship_hit_radius,
                engine_cfg.shot_velocity,
                float(engine_cfg.shot_lifetime),
            ]
        ):
            self._module.set_config_parameter(self._ctx, i, value)

    def clear_world_state(self):
        self._module.clear_world_state(self._ctx)

    def update_ship(self, agent_id, *, is_alive, pose):
        self._module.update_ship(
            self._ctx, agent_id, 1 if is_alive else 0, pose.x, pose.y, pose.heading
        )

    def update_shot(self, agent_id, *, lifetime, pose):
        self._module.update_ship(
            self._ctx, agent_id, lifetime, pose.x, pose.y, pose.heading
        )

    def update_score(self, agent_id, score):
        self._module.update_score(self._ctx, agent_id, score)

    def make_action(self, agent_id, action):
        return self._module.make_action(self._ctx, agent_id, action)
