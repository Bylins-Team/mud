# ISSUE-024: Buffer Overflow in ObjectFile - strcat Without Size Check

## Severity
**MEDIUM** - Buffer Overflow

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Line: 750

## Description
Unsafe `strcat` operation appends to buffer without checking remaining space.

## Vulnerable Code
```cpp
char m_buffer[kMaxStringLength];  // 32768 bytes

// Earlier (line 606):
sprintf(m_buffer, "object #%d", nr);

// Later (line 750):
strcat(m_buffer, ", after numeric constants\n"
       "...expecting 'E', 'A', '$', or next object number");
```

## Risk Analysis
1. **Initial Content**: Buffer starts with "object #NNNNNN" (~15-20 chars)
2. **Appended String**: ~68 characters fixed string
3. **Current Risk**: LOW - total is ~88 chars in 32KB buffer
4. **Future Risk**: If initial sprintf changes, overflow possible

## Why This Is Still a Problem
While currently safe, this pattern is dangerous because:
1. No size checking despite using unsafe `strcat`
2. Buffer size depends on earlier `sprintf` call
3. Code maintainability - easy to break in future changes
4. Violates defensive programming principles

## Recommended Fix
```cpp
// Option 1: Use snprintf for append
const size_t current_len = strlen(m_buffer);
snprintf(m_buffer + current_len,
         sizeof(m_buffer) - current_len,
         ", after numeric constants\n"
         "...expecting 'E', 'A', '$', or next object number");

// Option 2: Use safe string class
std::string buffer_str = fmt::format("object #{}", nr);
buffer_str += ", after numeric constants\n"
              "...expecting 'E', 'A', '$', or next object number";
```

## Impact
- Potential buffer overflow (low probability currently)
- Code maintainability issue
- Security regression risk

## References
- CWE-120: Buffer Copy without Checking Size of Input
- CWE-676: Use of Potentially Dangerous Function
