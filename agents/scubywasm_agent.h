#ifndef SCUBYWASM_AGENT_H
#define SCUBYWASM_AGENT_H

/**
 * \file scubywasm_agent.h
 * \brief Scubywasm agent ABI.
 *
 * Scubywasm executes user-provided bots ("agents") as WebAssembly (WASM)
 * modules. The host interacts with an agent module exclusively through the
 * functions declared in this header.
 *
 * \par Teams and per-ship agent IDs
 * A single WASM module controls a *team* with \c agent_multiplicity members.
 * Each team member controls exactly one ship, and is identified by a 32-bit
 * \c agent_id. For each \c agent_id there is exactly one ship, and at most one
 * active shot at a time. Consequently, ships and shots are referred to by their
 * respective \c agent_id (there are no separate ship/shot IDs in this ABI).
 *
 * \par Agent IDs are opaque 32-bit identifiers
 * The host provides \c agent_id values as 32-bit identifiers. They are neither
 * required to be zero-based nor sequential and the agent must not assume any
 * particular numbering scheme. If the agent implementation requires an
 * internal indexing scheme, it must build and maintain it explicitly.
 *
 * \par Opaque context handle (no global state)
 * ::init_agent() returns a pointer to an implementation-defined \c Context.
 * For the host, this pointer is an opaque handle that must only be passed back
 * to subsequent API calls and never dereferenced.
 *
 * The \c Context is the place to store all agent state. In particular:
 *  - Persistent bot state (e.g., strategy, per-ship memory) must live in
 *    \c Context.
 *  - The same WASM module may be used to play multiple games concurrently; the
 *    host distinguishes such instances by the \c Context pointer.
 *
 * Therefore, agent implementations should avoid module-level global mutable
 * state.
 *
 * \par Typical call pattern
 * A common per-round / per-tick call sequence is:
 * \code{.c}
 * struct Context *ctx = init_agent(n_agents, agent_multiplicity);
 *
 * set_config_parameter(ctx, CFG_SHIP_MAX_VELOCITY, ship_max_velocity);
 * set_config_parameter(ctx, CFG_SHOT_VELOCITY,     shot_velocity);
 * // ... other configuration parameters ...
 *
 * for (uint32_t tick = 0; tick < max_ticks; tick++)
 * {
 *     clear_world_state(ctx);
 *
 *     for (each ship in the world)
 *         update_ship(ctx,
 *                     ship.agent_id,
 *                     ship.is_alive ? 1 : 0,
 *                     ship.x,
 *                     ship.y,
 *                     ship.heading_deg);
 *
 *     for (each shot in the world)
 *         update_shot(ctx,
 *                     shot.agent_id,
 *                     shot.lifetime,
 *                     shot.x,
 *                     shot.y,
 *                     shot.heading_deg);
 *
 *     for (each agent_id in the world)
 *         update_score(ctx,
 *                      agent.id,
 *                      agent.score);
 *
 *     for (each agent_id controlled by this team)
 *     {
 *         uint32_t action = make_action(ctx, agent.id, tick);
 *         engine.set_agent_action(agent.id, action);
 *     }
 * }
 *
 * free_context(ctx);
 * \endcode
 *
 * \par Discovering the team’s agent IDs
 * The agent does not receive an explicit list of the \c agent_id values it
 * controls. Instead, the host calls ::make_action() once per tick for each
 * \c agent_id that belongs to the team controlled by this WASM module. Agents
 * that need a stable roster must infer and maintain the set of controlled
 * \c agent_id values from these calls (e.g., by recording each \c agent_id
 * observed in ::make_action()).
 *
 * \par Fuel metering and unresponsive agents
 * All agent interactions within a tick (including calls to
 * ::clear_world_state(), ::update_ship(), ::update_shot(), ::update_score(),
 * and ::make_action()) are metered in units of wasmtime fuel. Before each tick,
 * the host refuels the agent instance to a fixed budget; the agent must not
 * exceed this budget over the tick. If the fuel is exhausted during a tick, the
 * agent becomes unresponsive and the host will stop calling ::make_action()
 * for that agent for the remainder of the round.
 *
 * \par Coordinate conventions
 * The current engine convention is:
 *  - \c x and \c y live on the unit torus with \c x and \c y in [0, 1)
 *  - \c heading is in [0, 360) degrees with 0 deg = up, 90 deg = right,
 *    180 deg = down, and 270 deg = left.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Action bitmask.
 *
 * The host interprets the return value of ::make_action() as a bitwise OR of
 * these flags. Unless explicitly stated otherwise, flags are combinable.
 *
 * Dynamics (turn rate, max velocity, shot velocity, lifetimes, etc.) are
 * defined by the current configuration as provided via ::set_config_parameter()
 * using ::ConfigParameter.
 *
 * \c ACTION_TURN_LEFT and \c ACTION_TURN_RIGHT are logically mutually
 * exclusive. If an agent sets both, the host may ignore both, pick one
 * deterministically, or apply a host-defined tie-breaker.
 */
enum ActionFlags : unsigned int
{
    /** Do nothing this tick. */
    ACTION_NONE = 0x00u,

    /**
     * Enable thrust for this tick.
     *
     * Ship speed is binary: either zero or the configured maximum.
     * If \c ACTION_THRUST is set, the ship's velocity is set to 
     * \c CFG_SHIP_MAX_VELOCITY. If \c ACTION_THRUST is not set, the ship's
     * velocity is set to zero.
     */
    ACTION_THRUST = 0x01u,

    /**
     * Turn left for this tick.
     *
     * Turning is binary: if \c ACTION_TURN_LEFT is set, the ship's heading is
     * changed by \c CFG_SHIP_MAX_TURN_RATE degrees (left) for this tick. If it
     * is not set, no left turn is applied.
     */
    ACTION_TURN_LEFT = 0x02u,

    /**
     * Turn right for this tick.
     *
     * Turning is binary: if \c ACTION_TURN_RIGHT is set, the ship's heading is
     * changed by exactly \c CFG_SHIP_MAX_TURN_RATE degrees (right) for this
     * tick. If it is not set, no right turn is applied.
     */
    ACTION_TURN_RIGHT = 0x04u,

    /**
     * Fire a shot.
     *
     * Shots travel with velocity \c CFG_SHOT_VELOCITY and expire after
     * \c CFG_SHOT_LIFETIME ticks. Each \c agent_id may have at most one active
     * shot at a time; if a shot is already active, firing is ignored.
     */
    ACTION_FIRE = 0x08u,
};

/**
 * \brief Engine configuration parameters. 
 *
 * These parameters define the relevant game dynamics and constraints that
 * agents should use for planning (movement, turning, shooting, collision
 * avoidance, etc.).
 *
 * Configuration parameters are set exactly once during initialization (after
 * ::init_agent() and before the first tick). They are never changed thereafter.
 * Each round uses a fresh WASM instance, so agents must not rely on
 * configuration carrying over between rounds.
 */
enum ConfigParameter : unsigned int
{
    /**
     * Ship turn rate per tick (in degrees per tick).
     *
     * If \c ACTION_TURN_LEFT or \c ACTION_TURN_RIGHT is set, the ship's
     * heading is changed by this value.
     */
    CFG_SHIP_MAX_TURN_RATE = 0,

    /**
     * Ship speed when thrust is enabled (in torus-units per tick).
     *
     * If \c ACTION_THRUST is set, the ship's velocity is set to this value. If
     * \c ACTION_THRUST is not set, the ship's velocity is zero.
     */
    CFG_SHIP_MAX_VELOCITY = 1,

    /**
     * Ship hit radius (in torus-units).
     *
     * Ships are considered colliding/touching when their distance satisfies the
     * engine's collision criterion derived from this radius.
     */
    CFG_SHIP_HIT_RADIUS = 2,

    /**
     * Shot velocity (in torus-units per tick).
     *
     * Determines how far a shot advances per tick after \c ACTION_FIRE
     * succeeds.
     */
    CFG_SHOT_VELOCITY = 3,

    /**
     * Shot lifetime / end-of-life (in ticks).
     *
     * A shot is removed when its lifetime reaches zero.
     */
    CFG_SHOT_LIFETIME = 4,
};

/**
 * \brief Opaque per-instance agent context.
 *
 * The agent defines and owns \c Context. The host must treat pointers to
 * \c Context as opaque handles (never dereference them) and pass them back
 * unchanged to subsequent calls.
 *
 * All mutable agent state should be stored in \c Context (no module-level
 * global state), enabling multiple concurrent game instances via distinct
 * \c Context pointers.
 */
struct Context;

/**
 * \brief Create a new per-round agent context.
 *
 * \param n_agents Total number of agents in the world (across all teams).
 * \param agent_multiplicity Number of team members.
 *
 * \return A new \c Context instance, or \c NULL on failure.
 */
struct Context *init_agent(uint32_t n_agents, uint32_t agent_multiplicity);

/**
 * \brief Destroy an agent context created by ::init_agent().
 *
 * Releases all resources owned by \p ctx. The host will not use \p ctx after
 * this call.
 *
 * \param ctx Context pointer returned by ::init_agent().
 */
void free_context(struct Context *ctx);

/**
 * \brief Set an immutable configuration parameter.
 *
 * Called exactly once per parameter during initialization, before the first
 * tick.
 *
 * \param ctx Context pointer returned by ::init_agent().
 * \param param Parameter to set.
 * \param value Parameter value (units implied by \p param).
 */
void set_config_parameter(struct Context *ctx, enum ConfigParameter param, float value);

/**
 * \brief Clear all observations for the next tick.
 *
 * Called at the beginning of each tick, before any \c update_* calls.
 *
 * \param ctx Context pointer returned by ::init_agent().
 */
void clear_world_state(struct Context *ctx);

/**
 * \brief Provide the current state of a ship.
 *
 * Called once per ship per tick to stream the full world state.
 *
 * \param ctx Context pointer returned by ::init_agent().
 * \param agent_id 32-bit ID of the ship (and its controlling agent).
 * \param hp Ship "health": \c 1 if alive, \c 0 if not alive.
 * \param x Ship x-position on the unit torus.
 * \param y Ship y-position on the unit torus.
 * \param heading Ship heading in degrees.
 */
void update_ship(struct Context *ctx,
                 uint32_t agent_id,
                 int32_t hp,
                 float x,
                 float y,
                 float heading);

/**
 * \brief Provide the current state of a shot.
 *
 * Called once per (active) shot per tick to stream the full world state.
 *
 * Whether the host calls this function for dead shots (i.e., \c lifetime == 0)
 * is engine-defined.
 *
 * \param ctx Context pointer returned by ::init_agent().
 * \param agent_id 32-bit id of the shot owner (and associated ship).
 * \param lifetime Remaining lifetime in ticks. A value of \c 0 indicates that
 *        the shot is no longer active.
 * \param x Shot x-position on the unit torus.
 * \param y Shot y-position on the unit torus.
 * \param heading Shot heading in degrees.
 */
void update_shot(struct Context *ctx,
                 uint32_t agent_id,
                 int32_t lifetime,
                 float x,
                 float y,
                 float heading);

/**
 * \brief Provide the current score for one agent.
 *
 * Called once per agent per tick to stream the scores.
 *
 * \param ctx Context pointer returned by ::init_agent().
 * \param agent_id 32-bit agent ID.
 * \param score Current score.
 */
void update_score(struct Context *ctx, uint32_t agent_id, int32_t score);

/**
 * \brief Compute the action for one controlled team member.
 *
 * Called once per tick for each \c agent_id in the team.
 *
 * \param ctx Context pointer returned by ::init_agent().
 * \param agent_id 32-bit ID of the ship/agent to act for.
 * \param tick Current tick number.
 *
 * \return Bitmask of ::ActionFlags.
 */
uint32_t make_action(struct Context *ctx, uint32_t agent_id, uint32_t tick);

#ifdef __cplusplus
}
#endif
#endif // SCUBYWASM_AGENT_H
