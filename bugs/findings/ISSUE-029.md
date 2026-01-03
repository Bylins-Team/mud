# ISSUE-029: No Input Validation on Zone Command Arguments - Integer Overflow

## Severity
**MEDIUM** - Integer Overflow / Logic Error

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Lines: 1686-1728

## Description
Zone file command arguments are parsed without range validation, allowing integer overflow or out-of-range values that could cause unexpected behavior or crashes.

## Vulnerable Code
```cpp
// Parse zone commands (lines 1686-1728):
auto if_flag = 0;
auto error = 0;
zone.cmd[cmd_no].arg4 = -1;

if (strchr("MOEGPDTVQF", zone.cmd[cmd_no].command) == nullptr) {
    const auto count = sscanf(ptr, " %d %d %d ",
                              &if_flag,
                              &zone.cmd[cmd_no].arg1,
                              &zone.cmd[cmd_no].arg2);
    if (count != 3) {
        error = 1;
    }
} else {
    const auto count = sscanf(ptr, " %d %d %d %d %d", &if_flag,
                              &zone.cmd[cmd_no].arg1,
                              &zone.cmd[cmd_no].arg2,
                              &zone.cmd[cmd_no].arg3,
                              &zone.cmd[cmd_no].arg4);
    if (count < 4) {
        error = 1;
    }
}
zone.cmd[cmd_no].if_flag = if_flag;
```

## Risk Analysis
1. **No Range Checks**: Arguments are raw integers without validation
2. **Signed Integers**: Can be negative when positive expected
3. **Overflow Risk**: Very large values could overflow when used
4. **Type Usage**: Arguments used as:
   - Object/Mobile/Room VNUMs
   - Quantities
   - Probabilities (should be 0-100)
   - Room directions (should be 0-5)

## Potential Issues

### Issue 1: Negative Values
```
M 1 -1 100 1234  # Negative mob vnum
O 1 5678 -5 1234 # Negative max_existing
```

### Issue 2: Integer Overflow
```
M 1 2147483647 100 1234  # INT_MAX
G 1 2147483648 100       # Overflow to negative
```

### Issue 3: Invalid Directions
```
D 0 1234 999 1  # Direction 999 (valid range: 0-5)
```

### Issue 4: Invalid Probabilities
```
M 500 1234 100 5678  # if_flag=500 (should be 0-100?)
```

## Attack Complexity
**LOW** - Requires filesystem access to lib/world/zon/ files

## Exploitation Scenario
1. Attacker modifies zone file with out-of-range values
2. Server loads zone during boot
3. Invalid values cause:
   - Array index out of bounds when used as indices
   - Integer overflow in calculations
   - Logic errors in game mechanics
   - Crashes or undefined behavior

## Recommended Fix

### Add validation function:
```cpp
bool validate_zone_command_args(char command, int arg1, int arg2, int arg3, int arg4) {
    switch (command) {
        case 'M':  // Load mobile
            if (arg1 < 0 || arg1 >= kMaxProtoNumber) return false;  // mob vnum
            if (arg2 < -1 || arg2 > 10000) return false;            // max_existing
            if (arg3 < 0 || arg3 >= kMaxProtoNumber) return false;  // room vnum
            break;

        case 'O':  // Load object
            if (arg1 < 0 || arg1 >= kMaxProtoNumber) return false;  // obj vnum
            if (arg2 < -1 || arg2 > 10000) return false;            // max_existing
            if (arg3 < 0 || arg3 >= kMaxProtoNumber) return false;  // room vnum
            break;

        case 'D':  // Set door state
            if (arg1 < 0 || arg1 >= kMaxProtoNumber) return false;  // room vnum
            if (arg2 < 0 || arg2 >= EDirection::kMaxDirNum) return false; // direction
            if (arg3 < 0 || arg3 > 2) return false;                 // state
            break;

        // Add validation for other commands...

        default:
            break;
    }
    return true;
}

// In parsing loop:
zone.cmd[cmd_no].if_flag = if_flag;

if (!validate_zone_command_args(zone.cmd[cmd_no].command,
                                 zone.cmd[cmd_no].arg1,
                                 zone.cmd[cmd_no].arg2,
                                 zone.cmd[cmd_no].arg3,
                                 zone.cmd[cmd_no].arg4)) {
    log("SYSERR: Invalid arguments for zone command '%c' in %s, line %d",
        zone.cmd[cmd_no].command, full_file_name().c_str(), line_num);
    exit(1);
}
```

### Validate if_flag (probability):
```cpp
if (if_flag < 0 || if_flag > 100) {
    log("SYSERR: Zone command if_flag %d out of range [0-100] in %s, line %d",
        if_flag, full_file_name().c_str(), line_num);
    exit(1);
}
zone.cmd[cmd_no].if_flag = if_flag;
```

## Impact
- Integer overflow leading to undefined behavior
- Array index out of bounds
- Game logic errors
- Potential crashes or security issues

## References
- CWE-190: Integer Overflow or Wraparound
- CWE-129: Improper Validation of Array Index
- CWE-20: Improper Input Validation
