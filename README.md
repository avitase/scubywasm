[![Engine CI](https://github.com/avitase/scubywasm/actions/workflows/engine_ci.yml/badge.svg)](https://github.com/avitase/scubywasm/actions/workflows/engine_ci.yml)
[![Agents CI](https://github.com/avitase/scubywasm/actions/workflows/agents_ci.yml/badge.svg)](https://github.com/avitase/scubywasm/actions/workflows/agents_ci.yml)
[![codecov](https://codecov.io/gh/avitase/scubywasm/graph/badge.svg?token=BHHBQK7596)](https://codecov.io/gh/avitase/scubywasm)

# Scubywasm
Scubywasm is the rightful heir and modern reincarnation of the infamous [Scubywars](https://github.com/SuperTux88/scubywars.kernel) - this time with [WebAssembly (WASM)](https://webassembly.org/) because this is definitely a good idea.

Scubywasm is (certainly a good idea and) a game engine than runs bots, aka _agents_, provided as WASM modules.

> [!NOTE]
> **Shortcut for `server/` users:** You do not have to compile WASM locally just to try the Python tooling.
> The CI workflows publish ready-to-use artifacts for `engine.wasm` and a minimal valid agent (`simple_agent.wasm`).
> See [Prebuilt WASM artifacts (from CI)](#prebuilt-wasm-artifacts-from-ci).

The concept is: rather than transmitting agent actions, simply send the entire program!
This is fun and allows for a (1) fair, (2) rapid, and (3) massively parallel execution; concretely:
 1. Fair: We have a long history of building agents that read world states over the wire, analyze/compute actions locally, and finally send their actions back via TCP/IP. Typically, servers don't wait for clients, which gives an edge to clients with beefier machines and shorter cables. This blatant injustice is over! In Scubywasm, each agent is given a finite instruction budget (aka _fuel_ or _gas_) that cannot be exceeded, and since everything runs on the same silicon, unstable or brittle network connections are no longer an issue.
 2. Rapid: Synchronization and latency are no longer issues either. Nowadays, WASM execution is fast (broadly speaking), and running rounds (of the Scubywasm game) within seconds is possible, allowing for rapid, sampling-based inference of who has written the best bot/agent!
 3. Parallel: Since we have all the code in one place, nothing stops us from running multiple rounds in parallel (this problem is virtually embarrassingly parallel!), yielding even faster samples.

## The Game
Scubywasm is a _tick_-based simulation of spherical spaceships on a flat 2D unit torus.
That is: When you leave the 1x1 square on the left, you re-enter on the right, same for top and bottom.

Ships can fire shots.
Once a shot is fired by a ship, no more shots can be fired by this ship until the shot hits a target or silently vanishes once it reached its EoL.
Shots and ships move on straight lines with constant velocity because this turned out to be a good idea in Scubywars.
Agents control ship(s) by turning them left or right, stopping (set velocity to zero), accelerating to max. velocity, or shooting.
Action can be combined and take effect within one _tick_ of the game engine.

If shots or ships _touch_, they explode and scores are amended:
 - Shot of agent A hits ship of agent B: A earns 2 points, B loses 1 point
 - Ship of agent A hits ship of agent B: both lose 1 point (you really shouldn't do this)
 - Shot of agent A hits ship of agent A: A earns 1 point (please let us know if this makes sense to you; truth is: this was easiest to implement)

One agent can control multiple ships that form a team.
Rounds end when there is only a single team left or the max. number of ticks has passed.

Please feel free to reverse engineer further rules from our [game engine implementation](engine/engine.c), but don't expect them to be stable over time (cf. [Hyrum's law](https://www.hyrumslaw.com/)).

## How to write bots/agents?
Implementing your own agent is simple once you've figured out how to compile your favorite programming language into a WASM module.
See [agents/simple_agent.c](agents/simple_agent.c) for an example of a very simple valid agent that implements the required [API](https://avitase.github.io/scubywasm/).

## How to run a server?
The stuff under [engine/](engine/) is serious craftsmanship that we are proud of... not such much the scripts under [server/](server/).
Still, if you are in desperate need of one of the them, please don't hesitate to use and (to help us) to improve them.
The tools are written in Python and collected in a Python module `scubywasm`.
Install it locally either via pip:

```bash
cd scubywasm/server

python3 -m venv .venv
source .venv/bin/activate

python -m pip install -U pip
python -m pip install -e .

scubywasm --help
scubywasm-show --help
scubywasm-server --help
```

or via [uv](https://docs.astral.sh/uv/)

```bash
cd scubywasm/server
uv sync

uv run scubywasm --help
uv run scubywasm-show --help
uv run scubywasm-server --help
```

The help menus of the three scripts `scubywasm` (a simple runner of a single round), `scubywasm-show` (a reply viewer of a round), and `scubywasm-server` (a simple game server) should give you enough information on what they are and how to use them. (If not, we do accept PRs!)

In case you are wondering _where the hack do I get the engine WASM from!?_, either run

```bash
cd scubywasm/engine
make
```
this should compile the engine to `engine/build/engine.wasm` (of course this is also a WASM module!) or use ...

## Prebuilt WASM artifacts (from CI)

If you only want to run the Python tooling under `server/` (or quickly smoke-test the project), you do not have to build WASM locally.
The CI workflows publish ready-to-use artifacts:

- `engine.wasm` (the game engine)
- `simple_agent.wasm` (a minimal, valid agent implementation)

Where to get them:

1. Open the repository's **Actions** page on GitHub.
2. Select the workflow you need:
   - **Engine CI** (builds `engine.wasm`)
   - **Agents CI** (builds `simple_agent.wasm`)
3. Open the most recent **successful** run on `main`.
4. Download the **Artifacts** from that run (GitHub will provide a `.zip`) and extract it locally.
