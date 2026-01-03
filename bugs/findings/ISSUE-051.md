# ISSUE-051: HIGH - Infinite Loop Protection Insufficient (10000 iterations)

## Severity
**HIGH** - Denial of Service via resource exhaustion

## Component
`src/engine/scripting/dg_scripts.cpp` (script_driver loop protection)

## Description
The DG Scripts system implements loop protection that terminates loops after 10,000 iterations. While this prevents true infinite loops, **10,000 iterations is still enough to cause significant server lag or denial of service**.

Additionally, the protection has weaknesses:
1. Only applies to `while` and `foreach` loops
2. Each nested script call gets its own 10,000 iteration budget
3. 30-iteration soft limit only adds 1 pulse wait, insufficient for complex operations

### Vulnerable Code (dg_scripts.cpp:5619-5639)
```cpp
if ((*orig_cmd == 'w' && process_if(orig_cmd + 6, go, sc, trig, type))
    || (*orig_cmd == 'f' && process_foreach_done(orig_cmd + 8, go, sc, trig, type))) {
    cl = cl->original;
    loops++;
    GET_TRIG_LOOPS(trig)++;

    if (loops == 30) {
        snprintf(buf2, kMaxStringLength, "wait 1");
        process_wait(go, trig, type, buf2, cl);
        depth--;
        cur_trig = prev_trig;
        return ret_val;
    }
    if (GET_TRIG_LOOPS(trig) == 500) {
        trig_log(trig, "looping 500 times.", LGH);
    }
    if (GET_TRIG_LOOPS(trig) == 1000) {
        trig_log(trig, "looping 1000 times.", DEF);
    }
    if (GET_TRIG_LOOPS(trig) >= 10000) {
        trig_log(trig, fmt::format("looping 10000 times, cancelled"));
        loops = 0;
        cl = find_done(trig, cl);
    }
}
```

## Attack Scenarios

### Scenario 1: Simple Loop DoS
```
* Malicious trigger
while (%counter% < 9999)
  eval counter %counter% + 1
  * Do expensive operations here
  eval dummy %actor.room%
  eval dummy %actor.int%
  eval dummy %actor.str%
done
```

This will execute 9,999 times before any forced wait, causing lag spike.

### Scenario 2: Nested Script DoS
```
* Trigger 1
set counter 0
while (%counter% < 100)
  eval counter %counter% + 1
  run 1234  * Call another trigger
done

* Trigger 1234 (called 100 times)
set inner 0
while (%inner% < 9999)
  eval inner %inner% + 1
  * Expensive operations
done
```

Total iterations: 100 × 9,999 = 999,900 iterations across multiple script contexts.

### Scenario 3: Object Iteration DoS
```
* Trigger on common object (e.g., money)
foreach char /all/
  * This runs for EVERY character in the game
  eval dummy %char.name%
  eval dummy %char.room%
  eval dummy %char.level%
done
```

If there are 500 players online, this executes 500 iterations with multiple variable evaluations each.

### Scenario 4: Multiple Simultaneous Triggers
An attacker could set up 10 different triggers (on different mobs/objects) all executing near-10000 iteration loops simultaneously, multiplying the DoS effect.

## Root Cause Analysis

### Problem 1: High Iteration Limit
10,000 iterations per trigger is far too high for a real-time game server running at 25Hz (40ms per tick).

### Problem 2: Soft Limit Too Weak
After 30 iterations, only 1 pulse (40ms) wait is added. This is insufficient:
- Returns control too quickly
- Allows trigger to continue hammering the server
- No exponential backoff

### Problem 3: Local Loop Counter Reset
```cpp
unsigned long loops = 0;  // Line 5481
```

This is a **local variable** in `timed_script_driver()`, meaning each script invocation gets a fresh counter. Recursive triggers can bypass this.

### Problem 4: No Global Rate Limiting
There's no limit on:
- Total script execution time per pulse
- Number of active script contexts
- Script CPU usage across all triggers

### Problem 5: Warnings, Not Errors
```cpp
if (GET_TRIG_LOOPS(trig) == 500) {
    trig_log(trig, "looping 500 times.", LGH);
}
if (GET_TRIG_LOOPS(trig) == 1000) {
    trig_log(trig, "looping 1000 times.", DEF);
}
```

These are just logged, not actionable. By the time 500 iterations hit, damage is done.

## Performance Impact Calculation

Assuming each iteration takes minimal 0.01ms (very optimistic):
- 10,000 iterations = 100ms of CPU time
- At 25Hz game loop, this consumes 2.5 pulses worth of CPU
- With 10 such triggers, that's 1 second of lag

More realistic with variable evaluation (0.1ms per iteration):
- 10,000 iterations = 1000ms = **1 full second of lag**

## Evidence from Code

### Loop Counter Definition (dg_scripts.cpp:5481)
```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    int depth = 0;
    int ret_val = 1, stop = false;
    char cmd[kMaxTrglineLength];
    Script *sc = 0;
    unsigned long loops = 0;  // LOCAL - resets on each call
    Trigger *prev_trig;
```

### Recursion Depth Check (dg_scripts.cpp:5489-5492)
```cpp
if (depth > kMaxScriptDepth) {  // 512
    trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
    return ret_val;
}
```

This means you can have 512 nested calls, each with 10,000 iteration budget = potential 5,120,000 operations.

## Impact

### Direct Impact
- **Server Lag**: Triggers can cause multi-second lag spikes
- **DoS**: Multiple triggers can make server unresponsive
- **Pulse Skip**: Game loop can fall behind, missing pulses
- **Player Disconnection**: Lag can cause client timeouts

### Cascading Impact
- Combat timings disrupted
- Spell effects delayed
- Movement commands queued
- Other triggers delayed (compound effect)

## Exploitation Difficulty
**LOW** - Any builder can create high-iteration triggers.

## Recommended Fixes

### Fix 1: Reduce Loop Limit (CRITICAL)
```cpp
// Change from 10000 to 100
if (GET_TRIG_LOOPS(trig) >= 100) {
    trig_log(trig, fmt::format("looping 100 times, cancelled"));
    loops = 0;
    cl = find_done(trig, cl);
}
```

100 iterations is more than sufficient for legitimate use cases.

### Fix 2: Stricter Soft Limit with Exponential Backoff
```cpp
if (loops == 10) {
    // Force 1 pulse wait after 10 iterations
    snprintf(buf2, kMaxStringLength, "wait 1");
    process_wait(go, trig, type, buf2, cl);
    depth--;
    cur_trig = prev_trig;
    return ret_val;
}
if (loops == 20) {
    // Force 2 pulse wait
    snprintf(buf2, kMaxStringLength, "wait 2");
    process_wait(go, trig, type, buf2, cl);
    depth--;
    cur_trig = prev_trig;
    return ret_val;
}
```

### Fix 3: Global Script Execution Time Limit
Add per-pulse total script execution budget:

```cpp
// In heartbeat or script manager
static constexpr long MAX_SCRIPT_TIME_PER_PULSE = 10000; // 10ms
static long current_pulse_script_time = 0;

// In script_driver, measure execution time
auto start_time = std::chrono::steady_clock::now();
// ... execute script ...
auto end_time = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

current_pulse_script_time += duration;
if (current_pulse_script_time > MAX_SCRIPT_TIME_PER_PULSE) {
    trig_log(trig, "Global script time budget exceeded, deferring execution");
    // Defer remaining scripts to next pulse
}
```

### Fix 4: Per-Trigger Cumulative Counter
Instead of local `loops`, use trigger-level cumulative counter:

```cpp
// In Trigger class
int total_loops_executed;

// Reset only on trigger reload, not on each call
if (GET_TRIG_TOTAL_LOOPS(trig) > 10000) {
    trig_log(trig, "Trigger has exceeded lifetime loop budget, disabling");
    trig->halt();
}
```

### Fix 5: Runtime Monitoring Dashboard
Add command to monitor script performance:

```
> stat scripts
Total triggers active: 245
Scripts executed this pulse: 12
Total iterations this pulse: 3,245
Top CPU consumers:
  1. Trigger #12345 (mob #678): 2,100 iterations, 45ms
  2. Trigger #23456 (obj #789): 1,000 iterations, 18ms
```

## Additional Mitigations

### 1. Complexity Analysis
Add static analysis tool to detect potentially expensive loops before deployment:

```cpp
bool analyze_trigger_complexity(Trigger *trig) {
    int nested_loop_depth = 0;
    int max_nested_depth = 3; // Warn if more than 3 nested loops

    // Parse trigger commands and detect nested while/foreach
    // Return false if complexity too high
}
```

### 2. Builder Permission Levels
Restrict high-iteration loops to senior builders:
- Junior builders: max 50 iterations
- Senior builders: max 100 iterations
- Gods: max 500 iterations

### 3. Trigger Testing Sandbox
Add test mode where triggers execute in isolated environment with metrics:

```
> trigedit 1234
> test
Trigger executed:
  - 450 iterations in while loop (line 12)
  - 23ms execution time
  - WARNING: Approaches performance limits
```

## Testing Plan

1. **Performance Test**: Create trigger with 9,999 iteration loop, measure server lag
2. **Nested Test**: Create recursive triggers, measure total iterations
3. **Concurrent Test**: Trigger 10 high-iteration scripts simultaneously
4. **Recovery Test**: Verify server recovers after loop termination

## Related Issues
- ISSUE-052: Recursion depth limit might be exploitable
- Potential CPU-based DoS vectors in variable evaluation

## References
- `src/engine/scripting/dg_scripts.cpp:5619-5639` (loop protection)
- `src/engine/scripting/dg_scripts.cpp:5481` (local loops counter)
- `src/engine/scripting/dg_scripts.h:127` (kMaxScriptDepth = 512)

## Status
**OPEN** - Requires performance testing and limit adjustment
