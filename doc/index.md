# Scubywasm Agent API

This documentation describes the **agent C ABI** that a WebAssembly agent module must export so it can be driven by the Scubywasm host.

Although the ABI is expressed in C (for precise types and calling conventions), **agents do not have to be authored in C**.
You may write an agent in any language that can be compiled to WASM, as long as the resulting module exports the required entry points with compatible signatures and semantics.

## Start here

- **Authoritative API reference:** [scubywasm_agent.h](@ref scubywasm_agent_api)
- **Minimal working example:** [agents/simple_agent.c](@ref scubywasm_simple_agent_example)

## What the header provides

The header documentation is the single source of truth for:

- The required exported functions (the full API surface).
- Parameter and data type semantics.
- The host-driven **call pattern** (initialization, per-tick updates, action queries, shutdown).

If you implement the exported functions with the documented behavior, your WASM module is a valid Scubywasm agent.

## Minimal example implementation

\anchor scubywasm_simple_agent_example

The following file is a very small but valid implementation of the Agent API.
It is intentionally minimal and should be read as a reference for *what is required* rather than as a competitive bot.

\verbinclude agents/simple_agent.c
