# ISSUE-050: CRITICAL - Insufficient Privilege Escalation Protection in mforce/oforce/wforce Commands

## Severity
**CRITICAL** - Allows player privilege escalation

## Component
`src/engine/scripting/dg_mobcmd.cpp` (do_mforce)
`src/engine/scripting/dg_objcmd.cpp` (do_oforce)
`src/engine/scripting/dg_wldcmd.cpp` (do_wforce)

## Description
The `mforce`, `oforce`, and `wforce` commands check if the target is below immortal level (`GetRealLevel(victim) < kLvlImmortal`) before allowing command execution. However, this check has a critical flaw:

**The check only prevents forcing immortals, but does NOT prevent forcing regular players to execute privileged commands.**

### Vulnerable Code (dg_mobcmd.cpp:695-696)
```cpp
} else if (GetRealLevel(victim) < kLvlImmortal) {
    command_interpreter(victim, argument);
}
```

## Attack Scenario

### Scenario 1: Force Player to Execute Admin Commands
A malicious builder creates a trigger that forces a regular player to execute commands they shouldn't have access to:

```
* Trigger example
mforce %actor% advance self 34
```

While the player themselves cannot execute `advance`, when **forced** to do so, the command goes through `command_interpreter()` **as if the player typed it themselves**. The command permission check happens AFTER the force, creating a window for exploitation.

### Scenario 2: Inventory Theft
```
mforce %actor% give all.������� immortal_builder
```

Force players to give away their items.

### Scenario 3: Character Manipulation
```
mforce %actor% delete
mforce %actor% password newpass
```

Force players to delete their character or change password (if these commands exist).

## Root Cause
The vulnerability exists because:

1. **Force commands use `command_interpreter()`** which processes the command with the victim's privileges
2. **Only immortals are protected** - regular players can be forced
3. **No whitelist of safe commands** - ANY command can be forced
4. **No logging of forced commands** - attacks are invisible
5. **Triggers can be attached to ANY mob/obj/room** by builders

## Evidence

### mforce (dg_mobcmd.cpp:645-700)
```cpp
void do_mforce(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/, Trigger *trig) {
    // ... checks ...

    if (victim->IsNpc()) {
        if (mob_script_command_interpreter(victim, argument, trig)) {
            mob_log(ch, trig, "Mob trigger commands in mforce. Please rewrite trigger.");
            return;
        }
        command_interpreter(victim, argument);
    } else if (GetRealLevel(victim) < kLvlImmortal) {
        command_interpreter(victim, argument);  // VULNERABLE!
    }
    else
        mob_log(ch, trig, "mforce: ������� ��������� ������������.");
}
```

### oforce (dg_objcmd.cpp:220-221)
```cpp
} else if (GetRealLevel(ch) < kLvlImmortal) {
    command_interpreter(ch, line);  // VULNERABLE!
}
```

### wforce (dg_wldcmd.cpp:411-412)
```cpp
} else if (GetRealLevel(ch) < kLvlImmortal) {
    command_interpreter(ch, line);  // VULNERABLE!
}
```

## Impact

### Direct Impact
- **Privilege Escalation**: Players can be forced to execute commands beyond their access level
- **Item Theft**: Force players to give away equipment
- **Character Deletion**: Force players to delete themselves
- **Account Compromise**: Potentially force password changes
- **Economy Manipulation**: Force players to give gold, buy/sell items

### Cascading Impact
- **Complete Server Compromise**: If any admin command can be forced
- **Player Base Exodus**: If exploited, players will lose trust and leave
- **Data Loss**: Forced character deletion is permanent

## Exploitation Difficulty
**LOW** - Any builder with trigger creation privileges can exploit this.

## Who Can Exploit
- Zone builders (anyone who can create triggers)
- Possibly players if they can create portable quest items with triggers (needs verification)

## Recommended Fix

### Option 1: Command Whitelist (RECOMMENDED)
Only allow forcing of safe, non-privileged commands:

```cpp
// Define whitelist of safe commands
static const std::set<std::string> safe_force_commands = {
    "look", "inventory", "score", "who", "time", "weather",
    "north", "south", "east", "west", "up", "down"
    // Add other movement and info commands
};

bool is_safe_command(const char *command) {
    char cmd[kMaxInputLength];
    one_argument(command, cmd);
    utils::ConvertToLow(cmd);
    return safe_force_commands.find(cmd) != safe_force_commands.end();
}

void do_mforce(CharData *ch, char *argument, int cmd, int subcmd, Trigger *trig) {
    // ... existing checks ...

    if (victim->IsNpc()) {
        if (mob_script_command_interpreter(victim, argument, trig)) {
            mob_log(ch, trig, "Mob trigger commands in mforce. Please rewrite trigger.");
            return;
        }
        command_interpreter(victim, argument);
    } else if (GetRealLevel(victim) < kLvlImmortal) {
        // NEW: Check if command is safe
        if (!is_safe_command(argument)) {
            mob_log(ch, trig, "mforce: attempt to force unsafe command blocked");
            return;
        }
        command_interpreter(victim, argument);
    } else {
        mob_log(ch, trig, "mforce: ������� ��������� ������������.");
    }
}
```

### Option 2: Disable Force for Players Entirely
Never allow forcing of player characters:

```cpp
if (!victim->IsNpc()) {
    mob_log(ch, trig, "mforce: forcing players is disabled");
    return;
}
```

### Option 3: Add Permission Check INSIDE command_interpreter
Modify `command_interpreter()` to track if a command is being forced and apply stricter permission checks.

## Additional Security Measures

### 1. Audit Logging
```cpp
if (!victim->IsNpc() && GetRealLevel(victim) < kLvlImmortal) {
    sprintf(buf, "SECURITY: Player %s forced to execute: %s (trigger %d, mob %s)",
            GET_NAME(victim), argument, GET_TRIG_VNUM(trig), GET_NAME(ch));
    mudlog(buf, BRF, kLvlGod, SYSLOG, true);
    command_interpreter(victim, argument);
}
```

### 2. Trigger Permission Levels
Add permission levels to triggers - only god-level builders can create triggers with force commands.

### 3. Runtime Configuration
Add server config option to completely disable force commands:

```xml
<scripts>
    <allow_force_commands>false</allow_force_commands>
</scripts>
```

## Testing Recommendations

1. Create test trigger with `mforce %actor% advance self 34`
2. Verify if regular player can be forced to execute privileged commands
3. Test with various admin commands (delete, set, stat, etc.)
4. Check if forced commands bypass permission checks

## Related Issues
- Potentially related to command permission system
- May affect other script commands (eval, attach, run)

## References
- `src/engine/scripting/dg_mobcmd.cpp:645-700` (do_mforce)
- `src/engine/scripting/dg_objcmd.cpp:171-229` (do_oforce)
- `src/engine/scripting/dg_wldcmd.cpp:382-420` (do_wforce)
- `src/engine/ui/interpreter.cpp` (command_interpreter)

## Status
**OPEN** - Requires immediate attention
