# ISSUE-041: Integer Overflow and Memory Exhaustion in Object Loading

## Severity: HIGH

## Location
- File: `/home/kvirund/repos/mud/src/engine/db/obj_save.cpp`
- Function: `Crash_load_objs()` (lines 1350-1641)
- Related: `read_one_object_new()` (lines 137-534)

## Description

The object loading system allocates memory based on untrusted file size without proper bounds checking, allowing potential memory exhaustion attacks and integer overflows.

## Vulnerability Analysis

### Vulnerable Code (lines 1462-1474):

```cpp
fseek(fl, 0L, SEEK_END);
fsize = ftell(fl);  // User-controlled value from file size
if (!fsize) {
    // ...
}

CREATE(readdata, fsize + 1);  // DANGEROUS: no max size check
fseek(fl, 0L, SEEK_SET);
if (!fread(readdata, fsize, 1, fl) || ferror(fl) || !readdata) {
    // ...
}
```

### Problems:

1. **No Maximum Size Validation**: `fsize` can be arbitrarily large
   - Attacker can create huge player object files
   - `CREATE(readdata, fsize + 1)` allocates without limit
   - Can exhaust system memory

2. **Integer Overflow** in allocation:
   - If `fsize = SIZE_MAX`, then `fsize + 1` wraps to 0
   - `CREATE()` would allocate 0 bytes via `calloc(0, sizeof(char))`
   - Subsequent `fread()` writes to invalid memory → heap corruption

3. **No Rate Limiting**: Multiple players can load simultaneously
   - Each can trigger large allocations
   - DoS via coordinated memory exhaustion

4. **Item Count Trusting** (line 1496):
```cpp
for (fsize = 0, reccount = SAVEINFO(index)->rent.nitems;
     reccount > 0 && *data && *data != END_CHAR; reccount--, fsize++) {
```
- `rent.nitems` comes from timer file (also user-controlled)
- No validation that items count matches actual file content
- Could loop indefinitely if `nitems` is corrupted

## Attack Scenarios

### Scenario 1: Memory Exhaustion
```
1. Attacker creates player account
2. Modifies their object save file to be 2GB
3. Logs in
4. Server allocates 2GB for readdata
5. System runs out of memory → crash or DoS
```

### Scenario 2: Integer Overflow
```
1. Create object file with size = 0xFFFFFFFF (4GB on 32-bit)
2. fsize + 1 = 0x100000000 → wraps to 0 on 32-bit systems
3. calloc(0, 1) returns small/null pointer
4. fread() writes 4GB to small allocation → heap overflow
5. Heap metadata corruption → potential code execution
```

### Scenario 3: Infinite Loop
```
1. Modify timer file: set nitems = INT_MAX
2. Object file has only 1 item
3. Loop tries to read INT_MAX items
4. Hangs server thread
```

## Related Vulnerabilities

In `ReadCrashTimerFile()` (obj_save.cpp:1051-1151):
```cpp
fseek(fl, 0L, SEEK_END);
size = ftell(fl);  // No validation
// ...
if (count != num) {
    // Mismatch but continues anyway
}
```

## Impact

1. **Denial of Service**: Memory exhaustion crashes server
2. **Heap Corruption**: Integer overflow leads to heap overflow
3. **Performance Degradation**: Large allocations slow down server
4. **Resource Starvation**: Legitimate players can't load

## Exploitation Complexity

**MEDIUM** - Requires file system access:
1. Need to modify player save files on disk
2. OR exploit another vulnerability to write crafted files
3. Files are in `lib/plrobjs/` with specific permissions

## Current Mitigations

Partial:
- Files require specific permissions (lines 1991-1995)
- CRC checking via `FileCRC::check_crc()` (line 1477)
  - BUT CRC is checked AFTER allocation/read
  - Doesn't prevent memory exhaustion

## Proof of Concept

```cpp
// Crash_load_objs() vulnerable flow:
FILE *fl = fopen(fname, "r+b");  // Opens attacker-controlled file
fseek(fl, 0L, SEEK_END);
fsize = ftell(fl);  // fsize = 2GB

CREATE(readdata, fsize + 1);  // Allocates 2GB + 1 bytes
// Server memory exhausted → crash

// OR on 32-bit system:
// fsize = 0xFFFFFFFF
// fsize + 1 = 0x0 (integer overflow)
// CREATE(readdata, 0) → allocates minimal memory
// fread(readdata, 0xFFFFFFFF, 1, fl) → massive buffer overflow
```

## Recommended Fix

```cpp
int Crash_load_objs(CharData *ch) {
    char fname[kMaxStringLength], *data, *readdata;
    int fsize;

    // ... existing code ...

    fseek(fl, 0L, SEEK_END);
    long file_size_long = ftell(fl);

    // FIX 1: Validate file size before allocation
    const int MAX_OBJECT_FILE_SIZE = 10 * 1024 * 1024;  // 10MB limit

    if (file_size_long < 0) {
        log("SYSERR: ftell() failed for %s", fname);
        fclose(fl);
        return 1;
    }

    if (file_size_long > MAX_OBJECT_FILE_SIZE) {
        log("SYSERR: Object file too large for %s: %ld bytes (max %d)",
            GET_NAME(ch), file_size_long, MAX_OBJECT_FILE_SIZE);
        SendMsgToChar("Ваш файл объектов поврежден (слишком большой).\r\n", ch);
        fclose(fl);
        ClearCrashSavedObjects(index);
        return 1;
    }

    fsize = (int)file_size_long;

    if (fsize == 0) {
        // ... existing empty file handling ...
    }

    // FIX 2: Check for integer overflow before allocation
    if (fsize < 0 || fsize >= INT_MAX - 1) {
        log("SYSERR: Invalid file size for %s: %d", GET_NAME(ch), fsize);
        fclose(fl);
        ClearCrashSavedObjects(index);
        return 1;
    }

    // FIX 3: Validate item count
    const int MAX_ITEMS_PER_PLAYER = 1000;  // Reasonable limit
    if (SAVEINFO(index)->rent.nitems > MAX_ITEMS_PER_PLAYER) {
        log("SYSERR: Too many items for %s: %d (max %d)",
            GET_NAME(ch), SAVEINFO(index)->rent.nitems, MAX_ITEMS_PER_PLAYER);
        SendMsgToChar("Ваш файл объектов поврежден.\r\n", ch);
        ClearCrashSavedObjects(index);
        return 1;
    }

    // Now safe to allocate
    CREATE(readdata, fsize + 1);

    // ... rest of function ...
}
```

## Additional Recommendations

1. **Implement Global Memory Limits**:
   - Track total memory used by object loading
   - Reject new loads if system memory is low

2. **Add File Size Limits in Configuration**:
```cpp
// In configuration.xml or similar
max_player_object_file_size = 10485760  // 10MB
max_items_per_player = 1000
```

3. **Validate Before CRC Check**:
   - Size validation should come BEFORE reading entire file
   - Early rejection saves resources

4. **Rate Limit Object Loading**:
   - Limit concurrent object loads per IP/account
   - Prevents coordinated DoS

5. **Use mmap() Instead of fread()**:
   - Memory-map files instead of loading entire file
   - OS handles memory limits automatically

## References

- CWE-190: Integer Overflow or Wraparound
- CWE-770: Allocation of Resources Without Limits or Throttling
- CWE-789: Memory Allocation with Excessive Size Value

## Priority: HIGH

While exploitation requires file system access, the impact is severe (DoS, potential RCE). Fix before allowing untrusted users to upload/modify save files.
