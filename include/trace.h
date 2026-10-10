// Debug call tracing for the recompiled game code (enabled by "trace_mode = true" in us.toml).
// TRACE_ENTRY()/TRACE_RETURN() are emitted at the start/end of every recompiled function.
// Implemented in src/main/main.cpp (coop_trace_enter / coop_trace_leave + a watchdog that dumps the call stack when the game freezes).
#ifndef DK64_TRACE_H
#define DK64_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif
void coop_trace_enter(const char *name);
void coop_trace_leave(void);
#ifdef __cplusplus
}
#endif

#define TRACE_ENTRY() coop_trace_enter(__func__);
#define TRACE_RETURN() coop_trace_leave();

#endif
