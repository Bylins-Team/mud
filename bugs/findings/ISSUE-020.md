# ISSUE-020: Buffer Overflow in HelpFile::load_help() - strcpy/strcat Chain

## Severity
**CRITICAL** - Buffer Overflow

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Lines: 1776-1787

## Description
The `HelpFile::load_help()` function uses unsafe `strcpy` and `strcat` operations that can cause buffer overflow when processing help file entries.

## Vulnerable Code
```cpp
char key[READ_SIZE + 1], next_key[READ_SIZE + 1], entry[32384];
// ...
strcpy(entry, strcat(key, "\r\n"));  // Line 1776
get_one_line(line);
while (*line != '#') {
    // ...
    strcat(entry, strcat(line, "\r\n"));  // Line 1787
    get_one_line(line);
}
```

## Risk Analysis
1. **Buffer Size**: `entry` is 32384 bytes
2. **Unchecked Operations**: No bounds checking before `strcat` operations
3. **Attack Vector**: Malicious help file with extremely long entries
4. **Impact**: Stack-based buffer overflow leading to code execution

## Exploitation Scenario
1. Attacker creates malicious help file with >32384 character entry
2. Server loads help files during boot
3. Buffer overflow overwrites stack including return address
4. Attacker gains code execution as MUD server user

## Attack Complexity
**LOW** - Requires filesystem access to lib/text/help/ directory

## Recommended Fix
```cpp
// Use safe string operations with size limits
std::string entry_str;
entry_str.reserve(32384);

entry_str = key;
entry_str += "\r\n";

get_one_line(line);
while (*line != '#') {
    if (entry_str.size() + strlen(line) + 2 >= 32384) {
        log("SYSERR: Help entry too long in file %s, truncating", file_name().c_str());
        break;
    }
    entry_str += line;
    entry_str += "\r\n";
    get_one_line(line);
}
```

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-121: Stack-based Buffer Overflow
