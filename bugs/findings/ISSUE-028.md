# ISSUE-028: XML External Entity (XXE) Injection Risk in Config Manager

## Severity
**HIGH** - XML External Entity Injection

## Location
File: `/home/kvirund/repos/mud/src/engine/boot/cfg_manager.cpp`
Lines: 42-60

## Description
The configuration manager loads XML files using `parser_wrapper::DataNode` without apparent XXE protection. If the underlying XML parser doesn't disable external entities, this could lead to XXE attacks.

## Vulnerable Code
```cpp
void CfgManager::ReloadCfg(const std::string &id) {
    if (!loaders_.contains(id)) {
        err_log("некорректный параметр перезагрузки файла конфигурации (%s)", id.c_str());
        return;
    }
    const auto &loader_info = loaders_.at(id);
    auto data = parser_wrapper::DataNode(loader_info.file);  // XXE risk here
    loader_info.loader->Reload(data);
}

void CfgManager::LoadCfg(const std::string &id) {
    if (!loaders_.contains(id)) {
        err_log("некорректный параметр загрузки файла конфигурации (%s)", id.c_str());
        return;
    }
    const auto &loader_info = loaders_.at(id);
    auto data = parser_wrapper::DataNode(loader_info.file);  // XXE risk here
    loader_info.loader->Load(data);
}
```

## Risk Analysis
1. **XML Parser**: Uses `parser_wrapper::DataNode` for XML parsing
2. **External Entities**: If parser allows external entities, XXE possible
3. **Config Files Loaded**:
   - cfg/economics/currencies.xml
   - cfg/classes/pc_classes.xml
   - cfg/skills.xml
   - cfg/abilities.xml
   - cfg/spells.xml
   - cfg/feats.xml
   - cfg/guilds.xml
   - cfg/mob_classes.xml

## Attack Vectors

### Vector 1: File Disclosure via XXE
```xml
<!DOCTYPE foo [
  <!ENTITY xxe SYSTEM "file:///etc/passwd">
]>
<config>
  <value>&xxe;</value>
</config>
```

### Vector 2: Denial of Service (Billion Laughs)
```xml
<!DOCTYPE lolz [
  <!ENTITY lol "lol">
  <!ENTITY lol2 "&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;">
  <!ENTITY lol3 "&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;">
  <!-- ... -->
]>
<config>&lol9;</config>
```

### Vector 3: SSRF via External Entities
```xml
<!DOCTYPE foo [
  <!ENTITY xxe SYSTEM "http://internal-server/secret">
]>
<config>
  <value>&xxe;</value>
</config>
```

## Exploitation Scenario
1. Attacker gains write access to cfg/ directory
2. Modifies skills.xml to include XXE payload
3. Admin runs reload command or server restarts
4. XML parser processes external entity
5. File contents or HTTP response included in config data
6. Information disclosure or SSRF

## Attack Complexity
**MEDIUM** - Requires:
1. Write access to cfg/ directory files
2. Understanding of XML structure
3. Knowledge that parser doesn't disable XXE (needs verification)

## Verification Needed
Need to check `parser_wrapper::DataNode` implementation to confirm if external entities are disabled. Check for:
- `XML_PARSE_NOENT` flag (should NOT be set)
- `XML_PARSE_DTDLOAD` disabled
- External entity resolution disabled

## Recommended Fix

### If using libxml2:
```cpp
// In parser_wrapper implementation:
xmlParserOption options =
    XML_PARSE_NONET |      // Disable network access
    XML_PARSE_NOCDATA |    // Merge CDATA as text
    XML_PARSE_NOENT |      // Don't substitute entities
    XML_PARSE_DTDLOAD |    // Don't load DTD
    0;

xmlDoc* doc = xmlReadFile(filename, NULL, options);
```

### If using pugixml:
```cpp
// In parser_wrapper implementation:
pugi::xml_document doc;
unsigned int parse_flags =
    pugi::parse_default & ~pugi::parse_pi;  // Disable processing instructions

pugi::xml_parse_result result = doc.load_file(
    filename.c_str(),
    parse_flags
);
```

### Add validation in CfgManager:
```cpp
void CfgManager::LoadCfg(const std::string &id) {
    if (!loaders_.contains(id)) {
        err_log("некорректный параметр загрузки файла конфигурации (%s)", id.c_str());
        return;
    }

    const auto &loader_info = loaders_.at(id);

    // Validate file exists and is within expected directory
    if (!validate_config_file_path(loader_info.file)) {
        err_log("некорректный путь к файлу конфигурации: %s",
                loader_info.file.c_str());
        return;
    }

    try {
        auto data = parser_wrapper::DataNode(loader_info.file);
        loader_info.loader->Load(data);
    } catch (const std::exception& e) {
        err_log("ошибка загрузки конфигурации %s: %s", id.c_str(), e.what());
    }
}
```

## Impact
- Information disclosure (read arbitrary files)
- Server-Side Request Forgery (SSRF)
- Denial of Service (CPU/memory exhaustion)
- Potential remote code execution (via file upload + XXE)

## References
- CWE-611: Improper Restriction of XML External Entity Reference
- OWASP: XML External Entity (XXE) Processing
- OWASP Top 10 2017: A4-XML External Entities
