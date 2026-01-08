import argparse
import dataclasses
import functools
import pathlib
import random
import struct

import wasmtime


class WASMModule:
    def __init__(self, wasm, *, store):
        self._store = store
        self._instance = wasmtime.Instance(
            self._store,
            wasmtime.Module(self._store.engine, wasm),
            [],
        )

    def __getattr__(self, name):
        return functools.partial(self._instance.exports(self._store)[name], self._store)

    @property
    def store(self):
        return self._store

    def read_struct(self, fmt, ptr):
        memory = self._instance.exports(self._store)["memory"]
        return struct.unpack_from(fmt, memory.get_buffer_ptr(self._store), ptr)

    def write_struct(self, fmt, ptr, *values):
        memory = self._instance.exports(self._store)["memory"]
        memory.write(self._store, struct.pack(fmt, *values), ptr)


@dataclasses.dataclass(frozen=True)
class Config:
    ship_max_turn_rate: float
    ship_max_velocity: float
    ship_hit_radius: float
    shot_velocity: float
    shot_lifetime: int


@dataclasses.dataclass(frozen=True)
class Pose:
    x: float
    y: float
    heading: float


class Engine:
    def __init__(
        self,
        wasm,
        *,
        store,
        engine_cfg=None,
    ):
        self._module = WASMModule(wasm, store=store)
        self._ctx = self._create_context(engine_cfg)
        self._pose_ptr = self._module.get_pose_buffer()

    def _create_context(self, engine_cfg):
        cfg_ptr = self._module.get_config_buffer()

        cfg_keys = [
            "ship_max_turn_rate",
            "ship_max_velocity",
            "ship_hit_radius",
            "shot_velocity",
            "shot_lifetime",
        ]
        if engine_cfg is None:
            self._module.set_default_config(cfg_ptr)
            cfg_values = self._module.read_struct("<ffffi", cfg_ptr)
        else:
            cfg_as_dict = dataclasses.asdict(engine_cfg)
            self._module.write_struct(
                "<ffffi", cfg_ptr, *[cfg_as_dict[key] for key in cfg_keys]
            )

        self._cfg = Config(**{key: value for key, value in zip(cfg_keys, cfg_values)})

        return self._module.create_context(cfg_ptr)

    @property
    def config(self):
        return self._cfg

    def add_agent(self, pose):
        self._module.write_struct("<fff", self._pose_ptr, pose.x, pose.y, pose.heading)
        return self._module.add_agent(self._ctx, self._pose_ptr)

    def set_action(self, agent_id, action):
        self._module.set_action(self._ctx, agent_id, action)

    def tick(self, n_times):
        return self._module.tick(self._ctx, n_times)

    def get_ship_pose(self, agent_id):
        self._module.get_ship_pose(self._ctx, agent_id, self._pose_ptr)
        x, y, heading = self._module.read_struct("<fff", self._pose_ptr)
        return Pose(x=x, y=y, heading=heading)

    def get_shot_pose(self, agent_id):
        lifetime = self._module.get_shot_pose(self._ctx, agent_id, self._pose_ptr)
        x, y, heading = self._module.read_struct("<fff", self._pose_ptr)
        return Pose(x=x, y=y, heading=heading), lifetime

    def is_alive(self, agent_id):
        return self._module.is_alive(self._ctx, agent_id) == 1

    def get_score(self, agent_id):
        return self._module.get_score(self._ctx, agent_id)


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


class Game:
    def __init__(
        self,
        engine_wasm,
        agent_wasms,
        *,
        agent_multiplicity=1,
        init_poses=None,
        seed=None,
        engine_cfg=None,
    ):
        self.ticks = 0

        self._engine = Engine(
            engine_wasm, store=wasmtime.Store(), engine_cfg=engine_cfg
        )

        n, m = len(agent_wasms), agent_multiplicity
        agent_store = wasmtime.Store()
        agents = [
            Agent(
                agent_wasm,
                store=agent_store,
                n_agents_total=n * m,
                engine_cfg=self._engine.config,
            )
            for agent_wasm in agent_wasms
        ]

        if init_poses is not None:
            if len(init_poses) != n * m:
                raise ValueError(
                    f"Invalid number of initial positions ({len(init_poses)=}). Either pass None or {n * m}."
                )
        else:
            rng = random.Random(seed)
            phi = 1.61803398875  # golden ratio
            init_poses = [
                Pose(x=(i / phi) % 1.0, y=i / (n * m), heading=360 * rng.random())
                for i in range(n * m)
            ]
            rng.shuffle(init_poses)

        agent_ids = [self._engine.add_agent(pose) for pose in init_poses]
        batched_agent_ids = [agent_ids[i * m : (i + 1) * m] for i in range(n)]

        self._teams = [(agent, ids) for agent, ids in zip(agents, batched_agent_ids)]

    @property
    def config(self):
        return self._engine.config

    def tick(self, n_times=1):
        ships = dict()
        shots = dict()
        scores = dict()

        for agent, agent_ids in self._teams:
            agent.clear_world_state()

            for agent_id in agent_ids:
                is_alive = self._engine.is_alive(agent_id)
                ship_pose = self._engine.get_ship_pose(agent_id)
                if is_alive:
                    ships[agent_id] = {
                        "x": round(ship_pose.x, 4),
                        "y": round(ship_pose.y, 4),
                        "heading": round(ship_pose.heading, 1),
                    }

                shot_pose, lifetime = self._engine.get_shot_pose(agent_id)
                if lifetime > 0:
                    shots[agent_id] = {
                        "x": round(shot_pose.x, 4),
                        "y": round(shot_pose.y, 4),
                        "heading": round(shot_pose.heading, 1),
                    }

                score = self._engine.get_score(agent_id)
                scores[agent_id] = score

                for other, _ in self._teams:
                    other.update_ship(agent_id, is_alive=is_alive, pose=ship_pose)
                    other.update_shot(agent_id, lifetime=lifetime, pose=shot_pose)
                    other.update_score(agent_id, score=score)

        for agent, agent_ids in self._teams:
            for agent_id in agent_ids:
                action = agent.make_action(agent_id, self.ticks)
                self._engine.set_action(agent_id, action)

        n_alive = self._engine.tick(n_times)
        self.ticks += n_times

        return n_alive, {"ships": ships, "shots": shots, "scores": scores}


def main():
    parser = argparse.ArgumentParser(prog="Server")
    parser.add_argument("engine_wasmfile", type=pathlib.Path)
    parser.add_argument("agent_wasmfile", nargs="+", type=pathlib.Path)
    args = parser.parse_args()

    with open(args.engine_wasmfile, "rb") as f:
        engine_wasm = f.read()

    agent_wasms = []
    for file_name in args.agent_wasmfile:
        with open(file_name, "rb") as f:
            agent_wasms.append(f.read())

    game = Game(engine_wasm, agent_wasms)
    print(game.tick())


if __name__ == "__main__":
    main()
