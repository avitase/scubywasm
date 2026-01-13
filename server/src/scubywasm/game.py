import argparse
import json
import pathlib
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
                "scores": [],
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

            acc_score = 0
            for agent_id in agent_ids:
                is_alive = self._engine.is_alive(agent_id)

                ship_pose = self._engine.get_ship_pose(agent_id)
                team_alive[i] |= is_alive

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
                acc_score += score

                for other, _ in self._teams:
                    other.update_ship(agent_id, is_alive=is_alive, pose=ship_pose)
                    other.update_shot(agent_id, lifetime=lifetime, pose=shot_pose)
                    other.update_score(agent_id, score=score)

            self._log[i]["scores"].append(acc_score)

        for agent, agent_ids in self._teams:
            for agent_id in agent_ids:
                action = agent.make_action(agent_id, self.ticks)
                self._engine.set_action(agent_id, action)

        self._engine.tick(n_times)
        self.ticks += n_times

        return sum(team_alive)


def main():
    max_ticks = 1_000

    parser = argparse.ArgumentParser(prog="Scubywasm")
    parser.add_argument("engine_wasmfile", type=pathlib.Path)
    parser.add_argument("agent_wasmfile", nargs="+", type=pathlib.Path)
    parser.add_argument("--seed", type=int, help="Seed for random engine.")
    parser.add_argument(
        "--multiplicity", default=1, type=int, help="Agent multiplicity. (Default: 1)"
    )
    parser.add_argument(
        "--max_ticks",
        default=max_ticks,
        type=int,
        help=f"Max. number of ticks. (Default: {max_ticks})",
    )
    parser.add_argument(
        "--log_json",
        type=pathlib.Path,
        metavar="FILE",
        help="Write the game log (JSON) to FILE instead of stdout.",
    )

    args = parser.parse_args()
    max_ticks = max(1, args.max_ticks)

    with open(args.engine_wasmfile, "rb") as f:
        engine_wasm = f.read()

    agent_wasms = []
    for file_name in args.agent_wasmfile:
        with open(file_name, "rb") as f:
            agent_wasms.append(f.read())

    game = Game(
        engine_wasm, agent_wasms, seed=args.seed, agent_multiplicity=args.multiplicity
    )

    for _ in range(max_ticks):
        n_teams_alive = game.tick()
        if n_teams_alive <= 1:
            break

    if args.log_json is None:
        print(game.log)
    else:
        args.log_json.parent.mkdir(parents=True, exist_ok=True)
        with args.log_json.open("w", encoding="utf-8") as f:
            json.dump(game.log, f)
            f.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
