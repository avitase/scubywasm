import random

import wasmtime

from .agent import Agent
from .common import Pose
from .engine import Engine


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
                    f"Invalid number of initial positions ({len(init_poses)=}). "
                    f"Either pass None or {n * m}."
                )
        else:
            rng = random.Random(seed)
            phi = 1.61803398875  # golden ratio
            init_poses = [
                Pose(
                    x=(i / phi + 0.5) % 1.0,
                    y=(i / (n * m) + 0.5) % 1.0,
                    heading=360 * rng.random(),
                )
                for i in range(n * m)
            ]
            rng.shuffle(init_poses)

        agent_ids = [self._engine.add_agent(pose) for pose in init_poses]
        batched_agent_ids = [agent_ids[i * m : (i + 1) * m] for i in range(n)]

        self._teams = [(agent, ids) for agent, ids in zip(agents, batched_agent_ids)]
        self._log = [
            {
                "ships": {
                    agent_id: dict(x=[], y=[], heading=[], alive=[])
                    for agent_id in batch
                },
                "shots": {
                    agent_id: dict(x=[], y=[], lifetime=[]) for agent_id in batch
                },
                "scores": {agent_id: [] for agent_id in batch},
            }
            for batch in batched_agent_ids
        ]

    @property
    def config(self):
        return self._engine.config

    @property
    def log(self):
        return self._log

    def tick(self, n_times=1):
        team_alive = [False] * len(self._teams)
        for i, (agent, agent_ids) in enumerate(self._teams):
            agent.clear_world_state()

            for agent_id in agent_ids:
                is_alive = self._engine.is_alive(agent_id)

                ship_pose = self._engine.get_ship_pose(agent_id)
                team_alive[i] = True

                ship = self._log[i]["ships"][agent_id]
                ship["x"].append(round(ship_pose.x, 4))
                ship["y"].append(round(ship_pose.y, 4))
                ship["heading"].append(round(ship_pose.heading, 1))
                ship["alive"].append(is_alive)

                shot_pose, lifetime = self._engine.get_shot_pose(agent_id)
                shot = self._log[i]["shots"][agent_id]
                shot["x"].append(round(shot_pose.x, 4))
                shot["y"].append(round(shot_pose.y, 4))
                shot["lifetime"].append(lifetime)

                score = self._engine.get_score(agent_id)
                self._log[i]["scores"][agent_id].append(score)

                for other, _ in self._teams:
                    other.update_ship(agent_id, is_alive=is_alive, pose=ship_pose)
                    other.update_shot(agent_id, lifetime=lifetime, pose=shot_pose)
                    other.update_score(agent_id, score=score)

        for agent, agent_ids in self._teams:
            for agent_id in agent_ids:
                action = agent.make_action(agent_id, self.ticks)
                self._engine.set_action(agent_id, action)

        self._engine.tick(n_times)
        self.ticks += n_times

        return sum(team_alive)
