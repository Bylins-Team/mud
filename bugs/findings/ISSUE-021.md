# ISSUE-021: Buffer Overflow in SocialsFile::str_dup_bl() - Multiple strcat Calls

## Severity
**HIGH** - Buffer Overflow

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Lines: 1890-1901

## Description
The `SocialsFile::str_dup_bl()` function uses unsafe `strcat` operations without bounds checking.

## Vulnerable Code
```cpp
char *SocialsFile::str_dup_bl(const char *source) {
    char line[kMaxInputLength];  // 2048 bytes

    line[0] = 0;
    if (source[0]) {
        strcat(line, "&K");      // Line 1895
        strcat(line, source);    // Line 1896 - NO SIZE CHECK!
        strcat(line, "&n");      // Line 1897
    }

    return (str_dup(line));
}
```

## Risk Analysis
1. **Buffer Size**: `line` is `kMaxInputLength` (2048 bytes)
2. **Unchecked Input**: `source` parameter length is not validated
3. **Attack Vector**: Social message strings longer than ~2042 bytes
4. **Impact**: Stack-based buffer overflow

## Exploitation Scenario
1. Attacker creates malicious social file with very long message strings
2. During boot, server loads social messages
3. If any message > 2042 bytes, `strcat` overflows the buffer
4. Potential code execution or crash

## Attack Complexity
**LOW** - Requires filesystem access to lib/misc/socials file

## Recommended Fix
```cpp
char *SocialsFile::str_dup_bl(const char *source) {
    char line[kMaxInputLength];

    line[0] = 0;
    if (source[0]) {
        size_t source_len = strlen(source);
        if (source_len > kMaxInputLength - 5) {  // "&K" + "&n" + null
            log("SYSERR: Social message too long (%zu chars), truncating", source_len);
            source_len = kMaxInputLength - 5;
        }

        strncat(line, "&K", kMaxInputLength - 1);
        strncat(line, source, kMaxInputLength - strlen(line) - 1);
        strncat(line, "&n", kMaxInputLength - strlen(line) - 1);
        line[kMaxInputLength - 1] = '\0';
    }

    return (str_dup(line));
}
```

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-121: Stack-based Buffer Overflow
