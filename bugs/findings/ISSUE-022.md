# ISSUE-022: Format String Vulnerability in Multiple sprintf Calls

## Severity
**HIGH** - Format String Vulnerability

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Multiple locations (lines 214, 234, 238, 292, 455, 474, 514, 606, etc.)

## Description
Multiple uses of `sprintf` without size checking can lead to buffer overflows. While most don't directly include user-controlled format strings, the buffers are not size-checked.

## Vulnerable Code Examples

### Example 1: Line 214
```cpp
sprintf(line, "SYSERR: Trigger vnum #%d asked for but non-existant!", vnum);
```

### Example 2: Line 292
```cpp
sprintf(buf2, "trig vnum %d", vnum);
```

### Example 3: Line 514
```cpp
sprintf(buf2, "room #%d, direction D%u", GET_ROOM_VNUM(room), dir);
```

### Example 4: Line 606
```cpp
sprintf(m_buffer, "object #%d", nr);
```

## Risk Analysis
1. **No Size Checking**: `sprintf` doesn't validate buffer size
2. **Fixed Buffers**: Most use fixed-size buffers (256 bytes, etc.)
3. **Integer Values**: While using %d reduces risk, very large numbers could overflow
4. **Buffer Locations**: `buf`, `buf2`, `line`, `m_buffer` - various sizes

## Attack Complexity
**MEDIUM** - Requires ability to create objects/rooms with specific VNUMs

## Recommended Fix
Replace all `sprintf` calls with `snprintf`:

```cpp
// Instead of:
sprintf(line, "SYSERR: Trigger vnum #%d asked for but non-existant!", vnum);

// Use:
snprintf(line, sizeof(line), "SYSERR: Trigger vnum #%d asked for but non-existant!", vnum);
line[sizeof(line) - 1] = '\0';
```

### Complete Fix for Line 214:
```cpp
char line[kMaxTrglineLength];
snprintf(line, sizeof(line), "SYSERR: Trigger vnum #%d asked for but non-existant!", vnum);
line[sizeof(line) - 1] = '\0';
```

## Impact
- Buffer overflow
- Potential code execution
- System crash

## Affected Lines
- Line 214, 234, 238: Trigger error messages
- Line 292: Trigger vnum buffer
- Line 455, 474: Room error messages
- Line 514: Room direction buffer
- Line 606: Object number buffer
- Line 1523, 1745: Zone beginning messages

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-134: Use of Externally-Controlled Format String (indirect)
