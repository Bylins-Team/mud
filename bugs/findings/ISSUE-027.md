# ISSUE-027: Path Traversal Vulnerability in Data File Loading

## Severity
**HIGH** - Path Traversal / Directory Traversal

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/boot_data_files.cpp`
Lines: 36-43 (DataFile::open)

File: `/home/kvirund/repos/mud/src/engine/boot/boot_index.cpp`
Lines: 110-126 (FilesIndexFile::process_line)

## Description
The boot system loads files from index files without sanitizing filenames, allowing potential path traversal attacks. Filenames from index files are directly concatenated with prefixes and opened.

## Vulnerable Code

### boot_data_files.cpp:
```cpp
bool DataFile::open() {
    const std::string file_name = full_file_name();
    m_file = fopen(file_name.c_str(), "r");  // No path validation!
    if (nullptr == m_file) {
        log("SYSERR: %s: %s", file_name.c_str(), strerror(errno));
        exit(1);
    }
    return true;
}

// Called from:
virtual std::string full_file_name() const override {
    return prefixes(mode()) + file_name();  // Simple concatenation
}
```

### boot_index.cpp:
```cpp
int FilesIndexFile::process_line() {
    const auto &prefix = get_file_prefix();
    const std::string filename = prefix + line();  // Direct concatenation!
    m_entry_file.open(filename, std::ios::in);
    // ...
}
```

## Risk Analysis
1. **No Validation**: Filenames from index files are not validated
2. **Direct Concatenation**: Prefix + filename without path normalization
3. **Attack Vector**: Malicious index file entries with "../" sequences
4. **Impact**: Read arbitrary files from filesystem

## Attack Vectors

### Vector 1: Read Sensitive Files
```
# In lib/world/obj/index:
../../../etc/passwd
../../../../proc/self/environ
```

### Vector 2: Absolute Paths (if not filtered by prefix)
```
# In lib/world/mob/index:
/etc/shadow
/home/user/.ssh/id_rsa
```

## Exploitation Scenario
1. Attacker gains write access to index file (e.g., lib/world/obj/index)
2. Adds entry: `../../../../etc/passwd`
3. Full path becomes: `lib/world/obj/../../../../etc/passwd` = `/etc/passwd`
4. Server reads and parses /etc/passwd during boot
5. Information disclosure or crash from unexpected format

## Attack Complexity
**LOW** - Requires filesystem write access to lib/ directory index files

## Recommended Fix

### Add path validation function:
```cpp
// In boot_data_files.cpp or separate security module:

bool is_safe_filename(const std::string& filename) {
    // Check for directory traversal sequences
    if (filename.find("..") != std::string::npos) {
        return false;
    }

    // Check for absolute paths
    if (!filename.empty() && filename[0] == '/') {
        return false;
    }

    // Check for null bytes
    if (filename.find('\0') != std::string::npos) {
        return false;
    }

    // Normalize path and verify it's within expected directory
    std::filesystem::path normalized = std::filesystem::weakly_canonical(filename);
    std::filesystem::path expected_base = std::filesystem::current_path() / "lib";

    auto rel = std::filesystem::relative(normalized, expected_base);
    if (rel.string().find("..") != std::string::npos) {
        return false;
    }

    return true;
}

bool DataFile::open() {
    if (!is_safe_filename(m_file_name)) {
        log("SYSERR: Unsafe filename detected: %s", m_file_name.c_str());
        exit(1);
    }

    const std::string file_name = full_file_name();
    m_file = fopen(file_name.c_str(), "r");
    // ... rest of function
}
```

### Validate in index loading:
```cpp
int FilesIndexFile::process_line() {
    const auto &prefix = get_file_prefix();

    // Validate line content
    if (!is_safe_filename(line())) {
        log("SYSERR: Unsafe filename in index: %s", line().c_str());
        return 0;
    }

    const std::string filename = prefix + line();
    m_entry_file.open(filename, std::ios::in);
    // ...
}
```

## Impact
- Information disclosure
- Read arbitrary files
- Potential configuration file exposure
- Possible DoS from reading /dev/random or similar

## References
- CWE-22: Improper Limitation of a Pathname to a Restricted Directory ('Path Traversal')
- CWE-73: External Control of File Name or Path
- OWASP: Path Traversal
