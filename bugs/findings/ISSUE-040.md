# ISSUE-040: Path Traversal Vulnerability in get_filename()

## Severity: CRITICAL

## Location
- File: `/home/kvirund/repos/mud/src/engine/db/db.cpp`
- Function: `get_filename()` (lines 3021-3104)
- Related files: All player data save/load operations

## Description

The `get_filename()` function constructs file paths for player data without proper validation against path traversal attacks. While there is basic character filtering (lines 3058-3064), it's insufficient to prevent directory traversal.

## Vulnerability Analysis

### Current Code (lines 3058-3064):
```cpp
strcpy(name, orig_name);  // DANGEROUS: no bounds check
for (ptr = name; *ptr; ptr++) {
    if (*ptr == 'Я' || *ptr == 'я')
        *ptr = '9';
    else
        *ptr = LOWER(AtoL(*ptr));
}
```

### Problems:

1. **Buffer Overflow**: Uses unsafe `strcpy()` into fixed 64-byte buffer `name[64]` without checking `orig_name` length
2. **Path Traversal**: Only filters Cyrillic chars and converts to lowercase - does NOT filter:
   - `..` sequences
   - `/` or `\` directory separators
   - Null bytes
   - Special characters

3. **Unsafe sprintf** (line 3102):
```cpp
sprintf(filename, "%s%s" SLASH "%s.%s", prefix, middle, name, suffix);
```
No bounds checking - `filename` buffer size depends on caller

## Attack Scenarios

### Scenario 1: Directory Traversal
```
Player name: "../../etc/passwd"
After filtering: "../../etc/passwd" (unchanged - lowercase doesn't affect dots/slashes)
Result path: "lib/plrs/a-e/../../etc/passwd.plr"
```

### Scenario 2: Buffer Overflow
```
Player name: String of 1000 characters
strcpy() into name[64] → buffer overflow → potential code execution
```

### Scenario 3: Filename Injection
```
Player name: "test\0malicious"
Could bypass later filename checks
```

## Affected Operations

All player file operations use this function:
- Player save files (kPlayersFile)
- Crash/rent object files (kTextCrashFile, kTimeCrashFile)
- Alias files (kAliasFile)
- Script variables (kScriptVarsFile)
- Depot files (kPersDepotFile, kShareDepotFile, kPurgeDepotFile)

## Impact

1. **Read arbitrary files**: Attacker can read system files by crafting malicious player names
2. **Write arbitrary files**: Could overwrite system files during save operations
3. **Buffer overflow**: Potential remote code execution via crafted long names
4. **Data corruption**: Cross-contamination of player data files

## Exploitation Complexity

**LOW** - Easy to exploit:
1. Player registration accepts name as input
2. No validation of special characters in name
3. Direct construction of file paths from user input

## Proof of Concept

```cpp
// In ActualizePlayersIndex() (player_index.cpp:234-309)
char filename[kMaxStringLength];
if (get_filename(name, filename, kPlayersFile)) {
    // name comes from user input via players.lst
    // Could be "../../../tmp/evil.plr"
    LoadPlayerCharacter(name, short_ch, ...);
    // Loads from arbitrary path!
}
```

## Recommended Fix

```cpp
int get_filename(const char *orig_name, char *filename, int mode) {
    const char *prefix, *middle, *suffix;
    char name[64];

    if (orig_name == nullptr || *orig_name == '\0' || filename == nullptr) {
        log("SYSERR: NULL pointer or empty string passed to get_filename()");
        return 0;
    }

    // FIX 1: Validate input length BEFORE strcpy
    size_t len = strlen(orig_name);
    if (len == 0 || len >= sizeof(name)) {
        log("SYSERR: Invalid name length: %zu", len);
        return 0;
    }

    // FIX 2: Use safe string copy
    strncpy(name, orig_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    // FIX 3: Validate characters - reject path traversal attempts
    for (char *ptr = name; *ptr; ptr++) {
        // Reject dangerous characters
        if (*ptr == '.' || *ptr == '/' || *ptr == '\\' ||
            *ptr == '\0' || *ptr < 32 || *ptr > 126) {
            log("SYSERR: Invalid character in filename: %c (%d)", *ptr, *ptr);
            return 0;
        }

        // Convert to safe characters
        if (*ptr == 'Я' || *ptr == 'я')
            *ptr = '9';
        else
            *ptr = LOWER(AtoL(*ptr));
    }

    // FIX 4: Use snprintf with bounds checking
    int result = snprintf(filename, kMaxStringLength,
                         "%s%s" SLASH "%s.%s",
                         prefix, middle, name, suffix);

    if (result < 0 || result >= kMaxStringLength) {
        log("SYSERR: Filename buffer overflow");
        return 0;
    }

    return 1;
}
```

## Additional Recommendations

1. **Validate player names at registration** - Add whitelist validation in account creation
2. **Canonicalize paths** - Use `realpath()` to resolve and validate final paths
3. **Chroot environment** - Run MUD in chroot jail to limit filesystem access
4. **File permissions** - Ensure player data directories are not world-writable

## References

- CWE-22: Improper Limitation of a Pathname to a Restricted Directory ('Path Traversal')
- CWE-120: Buffer Copy without Checking Size of Input ('Classic Buffer Overflow')
- OWASP Path Traversal: https://owasp.org/www-community/attacks/Path_Traversal

## Priority: IMMEDIATE

This vulnerability allows arbitrary file read/write and potential code execution. Must be fixed before production deployment.
