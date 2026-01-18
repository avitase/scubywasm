import functools

from .wasmmodule import WASMModule


def fuel_guard(fn):
    @functools.wraps(fn)
    def wrapper(self, *args, **kwargs):
        if self._trapped:
            return None

        try:
            if self.fuel_level is None:
                return fn(self, *args, **kwargs)

            self._module.store.set_fuel(self._fuel_level)
            res = fn(self, *args, **kwargs)
            self._fuel_level = self._module.store.get_fuel()
            return res
        except Exception:
            self._trapped = True
            return None

    return wrapper


class Agent:
    def __init__(
        self,
        wasm,
        *,
        store,
        n_agents_total,
        agent_multiplicity,
        engine_cfg,
        fuel_capacity,
    ):
        self._module = WASMModule(wasm, store=store)

        self._trapped = False
        self._fuel_capacity = fuel_capacity
        self._fuel_level = fuel_capacity

        try:
            if self._fuel_capacity is not None:
                self._module.store.set_fuel(self._fuel_level)

            self._ctx = self._module.init_agent(n_agents_total, agent_multiplicity)

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

            if self._fuel_capacity is not None:
                self._fuel_level = self._module.store.get_fuel()
        except Exception:
            self._trapped = True

    def refuel(self):
        self._fuel_level = self._fuel_capacity

    @property
    def fuel_level(self):
        return self._fuel_level

    @property
    def trapped(self):
        return self._trapped

    @fuel_guard
    def clear_world_state(self):
        self._module.clear_world_state(self._ctx)

    @fuel_guard
    def update_ship(self, agent_id, *, is_alive, pose):
        self._module.update_ship(
            self._ctx, agent_id, 1 if is_alive else 0, pose.x, pose.y, pose.heading
        )

    @fuel_guard
    def update_shot(self, agent_id, *, lifetime, pose):
        self._module.update_shot(
            self._ctx, agent_id, lifetime, pose.x, pose.y, pose.heading
        )

    @fuel_guard
    def update_score(self, agent_id, score):
        self._module.update_score(self._ctx, agent_id, score)

    @fuel_guard
    def make_action(self, agent_id, ticks):
        return self._module.make_action(self._ctx, agent_id, ticks)
