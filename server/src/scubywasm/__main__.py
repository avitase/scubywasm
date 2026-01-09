import argparse
import pathlib

from .game import Game


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


raise SystemExit(main())
