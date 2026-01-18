import argparse
import json
import math
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
        agent_fuel_limit=None,
    ):
        self.ticks = 0

        self._engine = Engine(
            engine_wasm, store=wasmtime.Store(), engine_cfg=engine_cfg
        )

        n, m = len(agent_wasms), agent_multiplicity

        cfg = wasmtime.Config()
        cfg.consume_fuel = agent_fuel_limit is not None
        agent_store = wasmtime.Store(wasmtime.Engine(cfg))
        agents = [
            Agent(
                agent_wasm,
                store=agent_store,
                n_agents_total=n * m,
                agent_multiplicity=m,
                engine_cfg=self._engine.config,
                fuel_capacity=agent_fuel_limit,
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

            grid_size = math.ceil(math.sqrt(n * m))
            grid_spacing = 1.0 / grid_size
            init_poses = [
                Pose(
                    x=((i + rng.uniform(0.4, 0.6)) * grid_spacing),
                    y=((j + rng.uniform(0.4, 0.6)) * grid_spacing),
                    heading=360 * rng.random(),
                )
                for i in range(grid_size)
                for j in range(grid_size)
            ]
            rng.shuffle(init_poses)
            init_poses = init_poses[: n * m]

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
                "fuel": [],
            }
            for batch in batched_agent_ids
        ]

    @property
    def config(self):
        return self._engine.config

    @property
    def log(self):
        return {
            "ticks": self.ticks,
            "ship_hit_radius": round(self._engine.config.ship_hit_radius, 3),
            "history": self._log,
        }

    def tick(self, n_times=1):
        for agent, _ in self._teams:
            agent.refuel()
            agent.clear_world_state()

        team_alive = [False] * len(self._teams)
        for i, (_, agent_ids) in enumerate(self._teams):
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
                if action is not None:
                    self._engine.set_action(agent_id, action)

        for i, (agent, _) in enumerate(self._teams):
            self._log[i]["fuel"].append(agent.fuel_level)

        n_teams_alive = sum(team_alive)
        if n_teams_alive > 1:
            self._engine.tick(n_times)
            self.ticks += n_times

        return n_teams_alive


def main():
    max_ticks = 1_000

    parser = argparse.ArgumentParser(
        prog="scubywasm-run",
        description=(
            "Run a single Scubywasm match locally (engine + one or more agent WASM "
            "modules) and write the full game log as JSON. By default the JSON log is "
            "written to stdout. Use -o/--output to write it to a file."
        ),
    )
    parser.add_argument(
        "engine_wasmfile",
        type=pathlib.Path,
        metavar="ENGINE_WASM",
        help="engine WASM module (e.g. engine.wasm)",
    )
    parser.add_argument(
        "agent_wasmfile",
        nargs="+",
        type=pathlib.Path,
        metavar="AGENT_WASM",
        help="one or more agent/team WASM modules. Each module controls one team",
    )

    parser.add_argument(
        "--seed",
        type=int,
        metavar="SEED",
        help="seed for deterministic initialization",
    )
    parser.add_argument(
        "--multiplicity",
        default=1,
        type=int,
        metavar="N",
        help="number of ships per team (agent multiplicity)",
    )
    parser.add_argument(
        "--fuel_limit",
        type=int,
        metavar="FUEL",
        help=(
            "optional Wasmtime fuel limit per agent call. If unset, fuel metering is "
            "disabled"
        ),
    )
    parser.add_argument(
        "--max_ticks",
        default=max_ticks,
        type=int,
        metavar="T",
        help="maximum number of ticks to simulate",
    )
    parser.add_argument(
        "-o",
        type=pathlib.Path,
        dest="file_name",
        metavar="FILE",
        help="write the JSON log to FILE instead of stdout",
    )

    args = parser.parse_args()
    max_ticks = max(1, args.max_ticks)

    if args.multiplicity < 1:
        parser.error(f"--multiplicity must be >= 1 (got {args.multiplicity})")

    if args.fuel_limit is not None and args.fuel_limit < 100:
        parser.error(
            f"--fuel_limit must be (much) larger than 100 (got {args.fuel_limit})"
        )

    engine_wasm = args.engine_wasmfile.read_bytes()
    agent_wasms = [file_name.read_bytes() for file_name in args.agent_wasmfile]
    game = Game(
        engine_wasm,
        agent_wasms,
        seed=args.seed,
        agent_multiplicity=args.multiplicity,
        agent_fuel_limit=args.fuel_limit,
    )

    for _ in range(max_ticks):
        n_teams_alive = game.tick()
        if n_teams_alive <= 1:
            break

    if args.file_name is None:
        print(json.dumps(game.log))
    else:
        args.file_name.parent.mkdir(parents=True, exist_ok=True)
        with args.file_name.open("w") as f:
            json.dump(game.log, f)
            f.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
