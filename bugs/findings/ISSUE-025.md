# ISSUE-025: Unchecked sscanf Return Values - Format String Attack Vector

## Severity
**MEDIUM** - Input Validation Failure

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Multiple locations throughout file

## Description
Many `sscanf` calls check return value but some don't properly validate before using parsed data, and buffer sizes are not always enforced in the format string.

## Vulnerable Code Examples

### Example 1: Line 204 - Unchecked junk buffer
```cpp
char junk[8];  // Only 8 bytes!
get_line(file(), line);
count = sscanf(line, "%s %d", junk, &vnum);  // No size limit on %s!
```

### Example 2: Line 297 - No size limit on flags
```cpp
char flags[256];
k = sscanf(line, "%d %s %d %d", &attach_type, flags, &t, &add_flag);
// flags buffer could overflow if input has >255 char token
```

### Example 3: Line 632 - No size limit on f0
```cpp
char f0[256];
int parsed_entries = sscanf(m_line, " %s %d %d %d", f0, t + 1, t + 2, t + 3);
// f0 could overflow
```

## Risk Analysis
1. **Format String Issue**: `%s` with no width specifier reads unlimited data
2. **Buffer Sizes**: Fixed buffers (8, 128, 256 bytes)
3. **Attack Vector**: Malicious world/zone/object files with very long tokens
4. **Impact**: Buffer overflow during parsing

## Exploitation Scenario
1. Attacker creates malicious zone file with token >256 characters
2. Server parses file during boot
3. `sscanf` with `%s` writes beyond buffer end
4. Stack corruption leads to crash or code execution

## Attack Complexity
**LOW** - Requires filesystem access to lib/ directory

## Recommended Fix

### For all sscanf with %s:
```cpp
// Instead of:
char junk[8];
sscanf(line, "%s %d", junk, &vnum);

// Use:
char junk[8];
if (sscanf(line, "%7s %d", junk, &vnum) != 2) {  // 7 = sizeof-1
    log("SYSERR: Format error in trigger");
    return;
}
```

### Complete fix for Line 204:
```cpp
char junk[8];
char line[kMaxTrglineLength];
int vnum;

get_line(file(), line);
count = sscanf(line, "%7s %d", junk, &vnum);  // Limit to 7 chars + null

if (count != 2) {
    log("SYSERR: Error assigning trigger!");
    return;
}
```

## Affected Locations
- Line 204: `junk[8]` with `%s`
- Line 297: `flags[256]` with `%s`
- Line 431: `flags[128]` with `%s`
- Line 632: `f0[256]` with `%s`
- Line 674: `f0[256], f1[256], f2[256]` with `%s %s %s`
- Line 692: `f1[128], f2[128]` with `%s %s`
- Line 711: `f0[256]` with `%s`
- Line 1027: `f1[128], f2[128]` with `%s %s`
- Line 1625: `t1[80], t2[80]` with `%s %s`
- Line 1696: `t1[80], t2[80]` with `%s %s`

## Impact
- Stack-based buffer overflow
- Potential code execution
- Server crash during boot

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-134: Use of Externally-Controlled Format String
- CWE-787: Out-of-bounds Write
