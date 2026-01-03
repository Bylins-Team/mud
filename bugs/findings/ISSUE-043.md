# ISSUE-043: Race Condition in Player File Operations

## Severity: MEDIUM

## Location
- File: `/home/kvirund/repos/mud/src/engine/db/obj_save.cpp`
- Functions: `save_char_objects()` (lines 1855-2012), `Crash_load_objs()` (1350-1641)
- Related: `/home/kvirund/repos/mud/src/engine/db/player_index.cpp` - `ActualizePlayersIndex()` (234-310)

## Description

Player save/load operations are not properly synchronized, allowing race conditions between concurrent reads/writes to the same player's data files. This can lead to data corruption or duplication of items.

## Vulnerability Analysis

### Race Condition Scenarios:

#### 1. Concurrent Save Operations:

```cpp
// save_char_objects() - lines 1980-2000
if (get_filename(GET_NAME(ch), fname, kTextCrashFile)) {
    std::ofstream file(fname);  // Opens file
    if (!file.is_open()) {
        // Error handling
    }
    write_buffer << "\n$\n$\n";
    file << write_buffer.rdbuf();  // Writes data
    file.close();  // Closes file
    // No file locking!
}
```

**Problem:**
- No file locking between open and close
- Two threads can open same file simultaneously
- Both write → file corruption (interleaved writes)

#### 2. Read-While-Write Race:

```cpp
// Thread 1: Crash_load_objs() reading
FILE *fl = fopen(fname, "r+b");  // Opens for read
fseek(fl, 0L, SEEK_END);
fsize = ftell(fl);
// Reading file...

// Thread 2: save_char_objects() writing (concurrent)
std::ofstream file(fname);  // Truncates file!
// Writing new data...

// Thread 1 continues:
fread(readdata, fsize, 1, fl);  // Reads truncated/partial data → corruption
```

#### 3. Time-of-Check-Time-of-Use (TOCTOU):

```cpp
// Crash_delete_files() - lines 982-987
if (!(fl = fopen(filename, "rb"))) {
    // File doesn't exist - error
} else {
    fclose(fl);
    if (remove(filename) < 0) {  // TOCTOU: file could be recreated here
        // Error
    }
}
```

**Problem:**
- Check file exists
- File could be deleted/recreated by another thread
- Remove() operates on different file

#### 4. Player Index Race:

```cpp
// ActualizePlayersIndex() - player_index.cpp:277
player_table.Append(element);  // Not atomic

// Multiple threads could add same player twice
```

### No Locking Mechanisms:

Search for pthread_mutex, std::mutex, flock, lockf in db/ directory:
```bash
# Results: NONE found
# No file-level or data-structure locking implemented
```

## Attack Scenarios

### Scenario 1: Item Duplication
```
1. Player A logs out (triggers save_char_objects)
2. Network delay - save not yet complete
3. Player A reconnects quickly from different IP
4. Login process starts Crash_load_objs() while save is writing
5. Load reads partially written file
6. Items are duplicated or lost
```

### Scenario 2: File Corruption
```
1. Server crash triggers Crash_save_all_rent() for all players
2. Multiple threads save simultaneously
3. Two players with names starting with 'a' save concurrently
4. Both write to lib/plrobjs/a-e/ directory
5. Directory operations interleave → inode corruption
6. Files become unreadable
```

### Scenario 3: Data Loss via TOCTOU
```
1. Thread 1: Calls Crash_delete_files() for player
2. Thread 1: Opens file, confirms exists, closes
3. Thread 2: Player logs in, Crash_load_objs() opens same file
4. Thread 1: Calls remove() → deletes file while Thread 2 is reading
5. Thread 2: fread() fails → player loses all items
```

### Scenario 4: Index Corruption
```
1. Player "newbie" creates account
2. ActualizePlayersIndex("newbie") called by Thread 1
3. Simultaneously, admin runs player cleanup
4. FlushPlayerIndex() called by Thread 2
5. Both modify player_table simultaneously
6. Index becomes inconsistent
7. Players can't log in or data is cross-contaminated
```

## Impact

1. **Item Duplication**: Players can duplicate rare items
2. **Data Corruption**: Files become unreadable → player data loss
3. **Crash**: Invalid data read → server crash
4. **Data Loss**: Files deleted while being read
5. **Game Balance**: Exploited for economic advantage

## Exploitation Complexity

**MEDIUM** - Requires:
1. Knowledge of save timing
2. Ability to trigger concurrent operations (multi-login, crashes)
3. Some timing/luck

## Proof of Concept

```python
#!/usr/bin/env python3
# Item duplication exploit via race condition

import socket
import time
import threading

TARGET = "mud.server.com"
PORT = 4000

def login_and_logout(username, password):
    """Login, wait, logout - trigger save"""
    s = socket.socket()
    s.connect((TARGET, PORT))
    # ... send login commands ...
    s.send(f"{username}\n{password}\n".encode())
    time.sleep(0.5)  # Wait for item load
    s.send("quit\n".encode())  # Trigger save
    s.close()

def rapid_reconnect(username, password):
    """Reconnect quickly while save is happening"""
    time.sleep(0.1)  # Small delay to hit save window
    s = socket.socket()
    s.connect((TARGET, PORT))
    s.send(f"{username}\n{password}\n".encode())
    # If timing is right, items load before save completes
    # Items are now duplicated in memory and on disk
    time.sleep(1)
    s.send("quit\n".encode())
    s.close()

# Exploit: Logout and immediately reconnect
t1 = threading.Thread(target=login_and_logout, args=("duper", "pass123"))
t2 = threading.Thread(target=rapid_reconnect, args=("duper", "pass123"))

t1.start()
t2.start()

t1.join()
t2.join()

print("Race condition triggered - check for duplicated items")
```

## Current Mitigations

**None:**
- No file locking (flock, lockf, fcntl)
- No mutex/semaphore protection
- No transaction isolation
- CRC checking (FileCRC::check_crc) detects corruption but doesn't prevent it

## Recommended Fix

### 1. File-Level Locking:

```cpp
#include <sys/file.h>  // For flock()

class FileLock {
public:
    FileLock(const char *filename, int operation) : fd_(-1), locked_(false) {
        fd_ = open(filename, O_RDWR | O_CREAT, 0660);
        if (fd_ < 0) {
            log("SYSERR: Failed to open lock file %s", filename);
            return;
        }

        // Try to acquire lock with timeout
        struct timespec timeout = {.tv_sec = 5, .tv_nsec = 0};
        if (flock(fd_, operation | LOCK_NB) == 0) {
            locked_ = true;
        } else if (errno == EWOULDBLOCK) {
            log("WARNING: File %s is locked, waiting...", filename);
            // Wait for lock with timeout
            alarm(5);
            if (flock(fd_, operation) == 0) {
                locked_ = true;
                alarm(0);
            } else {
                log("SYSERR: Failed to acquire lock on %s: %s",
                    filename, strerror(errno));
            }
        }
    }

    ~FileLock() {
        if (locked_ && fd_ >= 0) {
            flock(fd_, LOCK_UN);
        }
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    bool is_locked() const { return locked_; }

private:
    int fd_;
    bool locked_;
};

// Usage in save_char_objects():
int save_char_objects(CharData *ch, int savetype, int rentcost) {
    char fname[kMaxStringLength];
    // ...

    if (get_filename(GET_NAME(ch), fname, kTextCrashFile)) {
        // Create lock file
        char lockfile[kMaxStringLength];
        snprintf(lockfile, sizeof(lockfile), "%s.lock", fname);

        FileLock lock(lockfile, LOCK_EX);  // Exclusive lock
        if (!lock.is_locked()) {
            log("SYSERR: Cannot acquire lock for %s", GET_NAME(ch));
            return false;
        }

        // Now safe to write
        std::ofstream file(fname);
        // ... write data ...

        // Lock automatically released on scope exit
    }
}
```

### 2. Player Index Locking:

```cpp
class PlayersIndex {
private:
    std::mutex index_mutex_;  // Add mutex

public:
    std::size_t Append(const PlayerIndexElement &element) {
        std::lock_guard<std::mutex> lock(index_mutex_);  // RAII lock
        const auto index = size();
        push_back(element);
        m_id_to_index.emplace(element.uid(), index);
        AddNameToIndex(element.name(), index);
        return index;
    }

    void SetName(const std::size_t index, const char *name) {
        std::lock_guard<std::mutex> lock(index_mutex_);
        // ... existing code ...
    }
};
```

### 3. Atomic File Operations:

```cpp
// Write to temp file, then atomic rename
int save_char_objects(CharData *ch, int savetype, int rentcost) {
    // ...

    char fname[kMaxStringLength];
    char tmpname[kMaxStringLength];

    if (get_filename(GET_NAME(ch), fname, kTextCrashFile)) {
        snprintf(tmpname, sizeof(tmpname), "%s.tmp.%d", fname, getpid());

        FileLock lock(...);  // Lock before writing

        // Write to temp file
        std::ofstream file(tmpname);
        if (!file.is_open()) {
            return false;
        }
        file << write_buffer.rdbuf();
        file.close();

        // Atomic rename (overwrites target)
        if (rename(tmpname, fname) != 0) {
            log("SYSERR: Failed to rename %s to %s: %s",
                tmpname, fname, strerror(errno));
            unlink(tmpname);
            return false;
        }
    }
}
```

### 4. Read Lock During Load:

```cpp
int Crash_load_objs(CharData *ch) {
    char fname[kMaxStringLength];

    if (get_filename(GET_NAME(ch), fname, kTextCrashFile)) {
        char lockfile[kMaxStringLength];
        snprintf(lockfile, sizeof(lockfile), "%s.lock", fname);

        FileLock lock(lockfile, LOCK_SH);  // Shared lock (allows concurrent reads)
        if (!lock.is_locked()) {
            SendMsgToChar("Не удается загрузить ваши вещи (файл заблокирован).\r\n", ch);
            return 1;
        }

        // Safe to read
        FILE *fl = fopen(fname, "rb");
        // ...
    }
}
```

## Additional Recommendations

1. **Implement Transaction Log**:
   - Log all save operations
   - Can replay on corruption

2. **Separate Lock Directory**:
   - Create `/lib/locks/` for lock files
   - Easier to monitor/clean

3. **Timeout Handling**:
   - If lock can't be acquired in 5 seconds, fail gracefully
   - Don't hang server

4. **Testing**:
   - Add stress tests with concurrent saves/loads
   - Verify file integrity after tests

5. **Monitoring**:
   - Log all lock failures
   - Alert on repeated lock timeouts

## References

- CWE-362: Concurrent Execution using Shared Resource with Improper Synchronization ('Race Condition')
- CWE-367: Time-of-check Time-of-use (TOCTOU) Race Condition
- POSIX flock(2): https://man7.org/linux/man-pages/man2/flock.2.html

## Priority: MEDIUM

While harder to exploit than other issues, race conditions can cause data corruption and item duplication. Should be fixed to ensure data integrity in production.
