# ISSUE-042: Unsafe Deserialization in read_one_object_new()

## Severity: HIGH

## Location
- File: `/home/kvirund/repos/mud/src/engine/db/obj_save.cpp`
- Function: `read_one_object_new()` (lines 137-534)
- Related: `write_one_object()` (lines 549-881)

## Description

The object deserialization system uses unsafe string parsing with `atoi()`, `sscanf()`, and unbounded string operations. Malformed object data can cause integer overflows, type confusion, and memory corruption.

## Vulnerability Analysis

### Vulnerable Patterns:

#### 1. Unchecked atoi() Conversions (multiple instances):

```cpp
// Line 158: vnum parsing
vnum = atoi(buffer);  // No validation of range
if (!vnum) {
    *error = 3;
    return object;
}

// Line 217: Location parsing
object->set_worn_on(atoi(buffer));  // Can be negative or huge

// Line 239: Timer parsing
object->set_timer(atoi(buffer));  // Can overflow

// Line 272-281: Object values
object->set_val(0, atoi(buffer));  // Type-specific values not validated
object->set_val(1, atoi(buffer));
object->set_val(2, atoi(buffer));
object->set_val(3, atoi(buffer));
```

**Problems:**
- `atoi()` returns 0 on error AND for "0" input - ambiguous
- No range checking (negative values, overflow)
- Type-specific validation missing (e.g., EObjType constraints)

#### 2. sscanf() Without Validation:

```cpp
// Line 305-333: Affects parsing
sscanf(buffer, "%d %d", t, t + 1);
object->set_affected(0, static_cast<EApply>(t[0]), t[1]);
```

**Problems:**
- No check if sscanf() succeeded (return value ignored)
- Direct cast to enum without validation
- `t[0]` could be any integer, cast to invalid EApply value

#### 3. Buffer Overflow in sscanf():

```cpp
// Line 409: Enchantment parsing
if (sscanf(tmp_buf.c_str(), "F %s", buf2) != 1) {
    *error = 54;
    return object;
}
```

**Problems:**
- `buf2` is global buffer, size unknown in this context
- No length limit on `%s` format
- Can overflow `buf2` if input is long

#### 4. Type Confusion:

```cpp
// Line 269: Object type
object->set_type(static_cast<EObjType>(atoi(buffer)));
```

**Problems:**
- No validation that value is valid EObjType enum
- Type determines how val[0-3] are interpreted
- Setting invalid type → type confusion → memory corruption

## Attack Scenarios

### Scenario 1: Integer Overflow in Timer
```
Object file contains:
Tmer: 2147483647~  (INT_MAX)

Result:
- object->set_timer(2147483647)
- Later: obj->dec_timer(timer_dec) causes overflow
- Negative timer bypasses deletion logic
- Object persists forever (resource leak)
```

### Scenario 2: Invalid Enum Cast
```
Object file contains:
Type: 999~  (invalid EObjType)

Result:
- set_type(static_cast<EObjType>(999))
- Type is out of valid range
- Later code switches on type → undefined behavior
- Could access invalid memory or crash
```

### Scenario 3: Buffer Overflow via sscanf
```
Object file contains:
Ench
F AAAA...[1000 A's]...AAAA~

Result:
- sscanf(tmp_buf.c_str(), "F %s", buf2)
- buf2 is kMaxStringLength (32768) BUT no limit in format
- If tmp_buf > 32768, buffer overflow
- Heap corruption
```

### Scenario 4: Affect Location Corruption
```
Object file contains:
Afc0: -999 100~

Result:
- sscanf(buffer, "%d %d", t, t+1) → t[0]=-999, t[1]=100
- set_affected(0, static_cast<EApply>(-999), 100)
- Invalid EApply value
- Later code uses as array index → out-of-bounds access
```

## Impact

1. **Type Confusion**: Invalid enum values cause undefined behavior
2. **Integer Overflow**: Timer/value overflows bypass game logic
3. **Memory Corruption**: Buffer overflows in sscanf()
4. **Denial of Service**: Crafted objects crash server
5. **Game Balance**: Invalid values bypass restrictions

## Exploitation Complexity

**MEDIUM** - Requires:
1. Access to modify player object files
2. Knowledge of object file format
3. Understanding of internal types/enums

## Proof of Concept

```
# Create malicious object file: /lib/plrobjs/a-e/attacker.objs

@ Items file
#1  (valid object vnum)
Type: 999~  (invalid EObjType - causes type confusion)
Val0: -2147483648~  (INT_MIN - causes integer overflow)
Val1: 2147483647~  (INT_MAX - causes integer overflow)
Tmer: -1~  (negative timer - bypasses deletion)
Afc0: 999 2147483647~  (invalid EApply enum and huge modifier)
Ench
F AAAAAAAAAAAAAAAAAAAAAA...[repeat 40000 times]...~  (buffer overflow)
W 2147483647~  (INT_MAX weight - causes overflow in calculations)
~
$
$

When player logs in:
1. Type 999 → type confusion → crash when type is used
2. Negative timer → object never deleted → resource leak
3. Invalid EApply 999 → array out-of-bounds → crash
4. Buffer overflow in sscanf → heap corruption
```

## Current Mitigations

**Weak:**
- Error codes set but many ignored (lines 183-500)
- Some validation (e.g., vnum != 0) but incomplete
- CRC checking happens AFTER parsing

## Recommended Fix

```cpp
ObjData::shared_ptr read_one_object_new(char **data, int *error) {
    // ... existing setup ...

    // FIX 1: Safe vnum parsing
    errno = 0;
    char *endptr;
    long vnum_long = strtol(buffer, &endptr, 10);

    if (errno == ERANGE || vnum_long < INT_MIN || vnum_long > INT_MAX) {
        log("SYSERR: Object vnum overflow: %s", buffer);
        *error = 3;
        return object;
    }

    if (endptr == buffer || *endptr != '\0') {
        log("SYSERR: Invalid vnum format: %s", buffer);
        *error = 3;
        return object;
    }

    vnum = (int)vnum_long;

    // Validate vnum range
    if (vnum < -1 || vnum > 999999) {  // Define MAX_VNUM
        log("SYSERR: Vnum out of valid range: %d", vnum);
        *error = 3;
        return object;
    }

    // ... create object ...

    // FIX 2: Validate object type
    if (!strcmp(read_line, "Type")) {
        *error = 28;
        int type_int = parse_safe_int(buffer);
        if (type_int < 0 || type_int >= EObjType::kMaxObjType) {
            log("SYSERR: Invalid object type: %d", type_int);
            return object;
        }
        object->set_type(static_cast<EObjType>(type_int));
    }

    // FIX 3: Validate timer
    else if (!strcmp(read_line, "Tmer")) {
        *error = 20;
        int timer = parse_safe_int(buffer);
        if (timer < -1 || timer > MAX_OBJ_TIMER) {  // Define limit
            log("SYSERR: Invalid timer: %d", timer);
            timer = MAX_OBJ_TIMER;
        }
        object->set_timer(timer);
    }

    // FIX 4: Safe sscanf with buffer limits
    else if (!strcmp(read_line, "Afc0")) {
        *error = 40;
        int location, modifier;
        if (sscanf(buffer, "%d %d", &location, &modifier) != 2) {
            log("SYSERR: Invalid affect format");
            return object;
        }

        // Validate EApply range
        if (location < 0 || location >= EApply::kMaxApply) {
            log("SYSERR: Invalid apply location: %d", location);
            return object;
        }

        // Validate modifier range
        if (modifier < -1000 || modifier > 1000) {  // Reasonable limit
            log("SYSERR: Affect modifier out of range: %d", modifier);
            modifier = std::clamp(modifier, -1000, 1000);
        }

        object->set_affected(0, static_cast<EApply>(location), modifier);
    }

    // FIX 5: Safe string parsing in enchantments
    else if (!strcmp(read_line, "Ench")) {
        // ... existing code ...
        case 'F': {
            char flag_buf[256];  // Fixed size
            if (sscanf(tmp_buf.c_str(), "F %255s", flag_buf) != 1) {  // Limit to 255
                *error = 54;
                return object;
            }
            tmp_aff.affects_flags_.from_string(flag_buf);
            break;
        }
    }
}

// Helper function
int parse_safe_int(const char *str) {
    errno = 0;
    char *endptr;
    long value = strtol(str, &endptr, 10);

    if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
        return 0;  // Or throw exception
    }

    if (endptr == str || *endptr != '\0') {
        return 0;
    }

    return (int)value;
}
```

## Additional Recommendations

1. **Schema Validation**:
   - Define strict schema for object files
   - Validate all fields before applying to object

2. **Sandboxed Parsing**:
   - Parse in isolated memory space
   - Validate before committing to game state

3. **Enum Range Constants**:
```cpp
enum class EObjType {
    kUndefined = 0,
    // ... types ...
    kMaxObjType  // Always last - for validation
};
```

4. **Fuzz Testing**:
   - Create fuzzer for object file parsing
   - Test with malformed inputs

## References

- CWE-502: Deserialization of Untrusted Data
- CWE-704: Incorrect Type Conversion or Cast
- CWE-129: Improper Validation of Array Index

## Priority: HIGH

Unsafe deserialization can lead to crashes and memory corruption. Must validate all untrusted input before use.
