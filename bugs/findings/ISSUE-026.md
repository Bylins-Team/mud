# ISSUE-026: Integer Overflow in Zone File Parsing - Array Index Out of Bounds

## Severity
**HIGH** - Memory Corruption

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Lines: 1666-1673

## Description
Zone file parsing uses `sscanf` to read integers into array indices without proper bounds checking, allowing out-of-bounds array access.

## Vulnerable Code
```cpp
// Earlier allocation based on counters (lines 1544-1556):
if (zone.typeA_count) {
    CREATE(zone.typeA_list, zone.typeA_count);
}

if (zone.typeB_count) {
    CREATE(zone.typeB_list, zone.typeB_count);
    CREATE(zone.typeB_flag, zone.typeB_count);
}

// Later access without bounds checking:
auto a_number = 0;
// ...
if (zone.cmd[cmd_no].command == 'A') {
    sscanf(ptr, " %d", &zone.typeA_list[a_number]);  // Line 1666
    a_number++;  // No check if a_number < zone.typeA_count!
    continue;
}

if (zone.cmd[cmd_no].command == 'B') {
    sscanf(ptr, " %d", &zone.typeB_list[b_number]);  // Line 1672
    b_number++;  // No check if b_number < zone.typeB_count!
    continue;
}
```

## Risk Analysis
1. **Count Phase**: First pass counts 'A' and 'B' commands (lines 1532-1540)
2. **Allocation**: Arrays allocated based on counts
3. **Parse Phase**: Second pass writes to arrays WITHOUT validating index
4. **Vulnerability**: If file modified between reads, or counting logic differs from parsing logic, overflow occurs

## Attack Vectors

### Vector 1: File Modification Race Condition
1. File has 5 'A' commands initially
2. Array allocated with size 5
3. File modified to have 10 'A' commands
4. Parser writes to indices 0-9, overflowing buffer

### Vector 2: Counting Logic Mismatch
1. First pass skips certain 'A' commands (e.g., in comments)
2. Array allocated too small
3. Second pass processes all 'A' commands
4. Buffer overflow on access beyond array bounds

## Exploitation Scenario
1. Attacker creates zone file with inconsistent 'A'/'B' command counts
2. Triggers either race condition or logic mismatch
3. Out-of-bounds write corrupts heap
4. Potential code execution or crash

## Attack Complexity
**MEDIUM** - Requires filesystem access and timing (for race) or understanding of parsing logic

## Recommended Fix
```cpp
auto a_number = 0;
auto b_number = 0;

// ... in parsing loop:

if (zone.cmd[cmd_no].command == 'A') {
    if (a_number >= zone.typeA_count) {
        log("SYSERR: Zone file %s has more 'A' commands than counted (%d)",
            full_file_name().c_str(), zone.typeA_count);
        exit(1);
    }
    sscanf(ptr, " %d", &zone.typeA_list[a_number]);
    a_number++;
    continue;
}

if (zone.cmd[cmd_no].command == 'B') {
    if (b_number >= zone.typeB_count) {
        log("SYSERR: Zone file %s has more 'B' commands than counted (%d)",
            full_file_name().c_str(), zone.typeB_count);
        exit(1);
    }
    sscanf(ptr, " %d", &zone.typeB_list[b_number]);
    b_number++;
    continue;
}
```

## Impact
- Heap-based buffer overflow
- Memory corruption
- Potential code execution
- Server crash

## References
- CWE-129: Improper Validation of Array Index
- CWE-787: Out-of-bounds Write
- CWE-122: Heap-based Buffer Overflow
