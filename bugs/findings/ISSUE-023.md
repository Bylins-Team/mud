# ISSUE-023: Buffer Overflow in ObjectFile::parse_object() - strcpy Without Bounds Check

## Severity
**HIGH** - Buffer Overflow

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Line: 617

## Description
The `ObjectFile::parse_object()` function uses unsafe `strcpy` to copy object short description into a fixed-size buffer without any size validation.

## Vulnerable Code
```cpp
tobj->set_short_description(utils::colorLOW(fread_string()));

strcpy(buf, tobj->get_short_description().c_str());  // Line 617 - VULNERABLE!
tobj->set_PName(ECase::kNom, utils::colorLOW(buf));
```

## Risk Analysis
1. **Buffer Used**: Global `buf` variable (likely MAX_STRING_LENGTH or smaller)
2. **Source**: `fread_string()` can return strings up to `kMaxStringLength` (32768 bytes)
3. **No Size Check**: Direct `strcpy` without validating string length
4. **Attack Vector**: Malicious object file with very long short description

## Data Flow
1. `fread_string()` reads unlimited string from file (up to kMaxStringLength)
2. String processed by `utils::colorLOW()`
3. Stored in object's short_description field
4. **VULNERABILITY**: `strcpy(buf, ...)` copies to fixed buffer

## Exploitation Scenario
1. Attacker creates object file with 32KB+ short description
2. Server loads object during boot
3. `strcpy` overflows the `buf` buffer
4. Stack/heap corruption leads to crash or code execution

## Attack Complexity
**LOW** - Requires filesystem access to lib/world/obj/ directory

## Recommended Fix
```cpp
// Option 1: Use safe string copy
const std::string& short_desc = tobj->get_short_description();
if (short_desc.length() >= sizeof(buf)) {
    log("SYSERR: Object #%d short description too long (%zu chars), truncating",
        nr, short_desc.length());
    strncpy(buf, short_desc.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
} else {
    strcpy(buf, short_desc.c_str());
}
tobj->set_PName(ECase::kNom, utils::colorLOW(buf));

// Option 2: Avoid buffer entirely
tobj->set_PName(ECase::kNom, utils::colorLOW(tobj->get_short_description()));
```

## Impact
- Stack-based buffer overflow
- Potential code execution
- Server crash during boot

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-121: Stack-based Buffer Overflow
- CWE-676: Use of Potentially Dangerous Function
