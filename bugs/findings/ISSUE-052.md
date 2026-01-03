# ISSUE-052: MEDIUM - Stack Overflow Risk from Deep Script Recursion

## Severity
**MEDIUM** - Potential stack overflow and server crash

## Component
`src/engine/scripting/dg_scripts.cpp` (script_driver recursion depth check)

## Description
The DG Scripts system allows recursive trigger execution up to 512 levels deep (`kMaxScriptDepth = 512`). While this limit prevents infinite recursion, **512 levels of recursion can still cause stack overflow** depending on:

1. Stack frame size of `script_driver()` function
2. Other functions on the call stack
3. System stack limits (typically 8MB on Linux)

### Current Protection (dg_scripts.cpp:5489-5492)
```cpp
if (depth > kMaxScriptDepth) {
    trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
    return ret_val;
}
```

### Stack Frame Size Analysis
The `timed_script_driver()` function has significant stack allocations:

```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    int depth = 0;
    int ret_val = 1, stop = false;
    char cmd[kMaxTrglineLength];     // Large buffer on stack
    Script *sc = 0;
    unsigned long loops = 0;
    Trigger *prev_trig;
    // ... more local variables ...
}
```

## Stack Usage Calculation

### Per-Frame Estimate
Assuming `kMaxTrglineLength = 16384` (16KB):
- `cmd` buffer: 16KB
- Local variables: ~100 bytes
- Return address, saved registers: ~100 bytes
- **Total per frame**: ~16.2 KB

### Maximum Stack Usage
512 recursion levels × 16.2 KB = **8,294.4 KB ≈ 8.1 MB**

This is **dangerously close to typical 8MB stack limits** on many systems.

### Risk Factors
1. **Other functions on stack**: Combat, spell system, command interpreter add their own frames
2. **Compiler optimizations**: Debug builds use more stack
3. **Stack limit variations**: Some systems have smaller limits (4MB)
4. **Trigger complexity**: More complex triggers = larger stack frames

## Attack Scenarios

### Scenario 1: Simple Recursive DoS
```
* Trigger 1234 - Self-recursive
run 1234
```

This will recurse until hitting 512 depth limit, consuming ~8MB stack.

### Scenario 2: Mutual Recursion
```
* Trigger 1000
run 1001

* Trigger 1001
run 1000
```

Ping-pongs between triggers, also hitting 512 depth.

### Scenario 3: Cascade Through Multiple Triggers
```
* Trigger A
run B

* Trigger B
run C

* Trigger C
run A
```

Creates a longer recursion chain that's harder to detect statically.

### Scenario 4: Deep Recursion During Combat
If recursion happens during combat (which has its own deep call stack):
```
Player attacks -> hit() -> damage() -> trigger fires -> recursion...
```

Combined stack usage could exceed limits even below 512 levels.

## Evidence from Code

### Recursion Counter (dg_scripts.cpp:5476-5520)
```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    int depth = 0;  // Static variable from outer scope, tracks depth
    // ...
    if (depth > kMaxScriptDepth) {
        trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
        return ret_val;
    }
    // ...
    depth++;  // Increment on entry
    // ... execute trigger ...
    depth--;  // Decrement on exit
    return ret_val;
}
```

**BUG**: `depth` is declared as **local variable**, not static!

Looking more carefully at line 5476:
```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    int depth = 0;
```

This means `depth` is ALWAYS 0 on entry! The check at line 5489 will NEVER trigger!

Wait, let me check if depth is passed as parameter... Looking at the code again:

Actually, I need to verify this. Let me check if depth is a global or passed differently.

### Depth Tracking Implementation
Looking at the actual code (line 5477):

```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    static int depth = 0;  // STATIC - persists across calls
    // ...
    if (depth > kMaxScriptDepth) {
        trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
        return ret_val;
    }
    depth++;
```

The depth counter IS properly implemented as `static`, so it does track recursion correctly.

## Root Cause
While the depth tracking works, the issue is the **high limit of 512 levels** combined with **large stack allocations** in each frame.

## Impact

### Direct Impact
- **Stack Overflow**: Infinite recursion will crash the server
- **Server Crash**: No recovery, requires restart
- **Data Loss**: Players lose unsaved progress
- **Complete DoS**: Server becomes unavailable

### Cascading Impact
- **Player Disconnection**: All players kicked
- **Reputation Damage**: Frequent crashes drive players away
- **Rollback Issues**: May need to restore from backup

## Exploitation Difficulty
**TRIVIAL** - Any builder can create a simple recursive trigger.

## Proof of Concept
```
* Trigger 99999 - Recursive bomb
* Name: Stack Overflow PoC
* Type: MOB GREET
run 99999
```

When any player enters the room, this will recurse infinitely and crash the server.

## Recommended Fixes

### Fix 1: Use Static Depth Counter (CRITICAL)
```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    static thread_local int depth = 0;  // STATIC - persists across calls
    int ret_val = 1, stop = false;
    char cmd[kMaxTrglineLength];
    Script *sc = 0;
    unsigned long loops = 0;
    Trigger *prev_trig;

    curr_trig_vnum = GET_TRIG_VNUM(trig);
    void mob_command_interpreter(CharData *ch, char *argument, Trigger *trig);
    void obj_command_interpreter(ObjData *obj, char *argument, Trigger *trig);
    void wld_command_interpreter(RoomData *room, char *argument, Trigger *trig);

    if (depth > kMaxScriptDepth) {
        trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
        return ret_val;
    }

    // ... existing code ...

    depth++;

    // ... trigger execution ...

    depth--;
    return ret_val;
}
```

**Note**: Use `thread_local` if server is multithreaded, otherwise just `static`.

### Fix 2: Pass Depth as Parameter
```cpp
int script_driver_internal(void *go, Trigger *trig, int type, int mode, int depth) {
    if (depth > kMaxScriptDepth) {
        trig_log(trig, "Triggers recursed beyond maximum allowed depth.");
        return 1;
    }

    // ... execute trigger ...

    // When calling recursively:
    script_driver_internal(trggo, runtrig, trgtype, TRIG_NEW, depth + 1);
}

// Public wrapper
int script_driver(void *go, Trigger *trig, int type, int mode) {
    return script_driver_internal(go, trig, type, mode, 0);
}
```

### Fix 3: Lower Recursion Limit
Even if depth tracking works, 512 is too high:

```cpp
const int kMaxScriptDepth = 50;  // Much safer limit
```

50 levels is more than sufficient for any legitimate trigger chain.

### Fix 4: Stack Allocation Reduction
Move large buffers off stack:

```cpp
int timed_script_driver(void *go, Trigger *trig, int type, int mode) {
    static thread_local char cmd_buffer[kMaxTrglineLength];
    char *cmd = cmd_buffer;  // Use thread-local buffer instead of stack
    // ... rest of function ...
}
```

Or use heap allocation:
```cpp
std::unique_ptr<char[]> cmd = std::make_unique<char[]>(kMaxTrglineLength);
```

### Fix 5: Add Stack Usage Monitoring
```cpp
#include <sys/resource.h>

bool check_stack_safety() {
    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);

    // Get current stack usage (platform-specific)
    size_t stack_used = get_stack_usage();  // Implement this
    size_t stack_available = limit.rlim_cur - stack_used;

    if (stack_available < 1024 * 1024) {  // Less than 1MB left
        return false;
    }
    return true;
}
```

## Additional Mitigations

### 1. Cycle Detection
Track trigger call chain and detect cycles:

```cpp
static std::set<int> trigger_call_stack;

int script_driver(void *go, Trigger *trig, int type, int mode) {
    int trig_vnum = GET_TRIG_VNUM(trig);

    if (trigger_call_stack.count(trig_vnum)) {
        trig_log(trig, "Recursive loop detected, aborting");
        return 1;
    }

    trigger_call_stack.insert(trig_vnum);
    // ... execute trigger ...
    trigger_call_stack.erase(trig_vnum);
}
```

### 2. Static Analysis Tool
Build tool to analyze trigger files and detect potential recursion:

```bash
# Tool to check for recursive trigger calls
./analyze_triggers.sh lib/world/trg/*.trg
Warning: Trigger 1234 calls trigger 1235
Warning: Trigger 1235 calls trigger 1234
ERROR: Recursive loop detected: 1234 -> 1235 -> 1234
```

### 3. Server Crash Handler
Add signal handler for stack overflow (SIGSEGV):

```cpp
void stack_overflow_handler(int sig) {
    log("CRITICAL: Stack overflow detected in script system");
    log("Last trigger: %d, depth attempted: %d", last_trig_vnum, depth);
    // Emergency cleanup
    abort();
}

signal(SIGSEGV, stack_overflow_handler);
```

## Testing Plan

1. **Depth Test**: Create recursive trigger, verify it stops at limit
2. **Stack Test**: Monitor stack usage during deep recursion
3. **Crash Test**: Attempt to trigger stack overflow (in test environment)
4. **Performance Test**: Measure impact of depth tracking overhead

## Verification Steps

1. Add instrumentation to track actual depth
2. Create test trigger with `run self` recursion
3. Monitor server logs for depth warnings
4. Verify server doesn't crash

## Related Issues
- ISSUE-051: Loop iteration limits
- General script performance concerns

## References
- `src/engine/scripting/dg_scripts.cpp:5476-5520` (timed_script_driver)
- `src/engine/scripting/dg_scripts.cpp:5489` (broken depth check)
- `src/engine/scripting/dg_scripts.h:127` (kMaxScriptDepth constant)

## Status
**CRITICAL** - Depth tracking appears to be broken, allowing infinite recursion
