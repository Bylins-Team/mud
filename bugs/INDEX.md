# Индекс найденных проблем безопасности и ошибок

Этот файл содержит оглавление всех найденных проблем. Полное описание каждой проблемы находится в `findings/ISSUE-XXX.md`.

## Статистика

**Всего проблем**: В процессе подсчёта...
**Критичность**:
- CRITICAL: В процессе подсчёта...
- HIGH: В процессе подсчёта...
- MEDIUM: В процессе подсчёта...
- LOW: В процессе подсчёта...

## Список проблем по критичности

### CRITICAL (Критические - требуют немедленного исправления)

- [ISSUE-001](findings/ISSUE-001.md) - Command Injection в функциях отправки паролей (`password.cpp`)

### HIGH (Высокая - исправить в ближайшее время)

- [ISSUE-002](findings/ISSUE-002.md) - Передача паролей открытым текстом по email (`password.cpp`)
- [ISSUE-006](findings/ISSUE-006.md) - Использование неинициализированного глобального буфера (`ban.cpp`)

### MEDIUM (Средняя - исправить в плановом порядке)

- [ISSUE-003](findings/ISSUE-003.md) - Path Traversal в сохранении аккаунтов (`accounts.cpp`)
- [ISSUE-004](findings/ISSUE-004.md) - Потенциальный out-of-bounds доступ (`accounts.cpp`)
- [ISSUE-007](findings/ISSUE-007.md) - Потенциальное переполнение буфера в strcpy (`names.cpp`)
- [ISSUE-008](findings/ISSUE-008.md) - Небезопасное использование sscanf с фиксированными буферами (`names.cpp`)
- [ISSUE-010](findings/ISSUE-010.md) - Некорректная логика в DisconnectBannedIp (`ban.cpp`)
- [ISSUE-011](findings/ISSUE-011.md) - Использование raw pointer вместо shared_ptr (`names.cpp`)

### LOW (Низкая - улучшение качества кода)

- [ISSUE-005](findings/ISSUE-005.md) - Мёртвый/закомментированный код (`accounts.cpp`)
- [ISSUE-009](findings/ISSUE-009.md) - Memory leak в ReadCharacterInvalidNamesList (`names.cpp`)
- [ISSUE-012](findings/ISSUE-012.md) - Отсутствие проверки размера массива в ReloadBan/ReloadProxyBan (`ban.cpp`)

## Список проблем по модулям

### src/administration/

#### password.cpp
- ISSUE-001 (CRITICAL) - Command Injection
- ISSUE-002 (HIGH) - Передача паролей открытым текстом

#### accounts.cpp
- ISSUE-003 (MEDIUM) - Path Traversal
- ISSUE-004 (MEDIUM) - Out-of-bounds доступ
- ISSUE-005 (LOW) - Мёртвый код

#### ban.cpp
- ISSUE-006 (HIGH) - Глобальный буфер race condition
- ISSUE-010 (MEDIUM) - Некорректная логика DisconnectBannedIp
- ISSUE-012 (LOW) - Нет проверки размера массива

#### names.cpp
- ISSUE-007 (MEDIUM) - Buffer overflow в strcpy
- ISSUE-008 (MEDIUM) - Небезопасный sscanf
- ISSUE-009 (LOW) - Memory leak
- ISSUE-011 (MEDIUM) - Raw pointer вместо shared_ptr

### src/engine/boot/
*Анализ в процессе...*

### src/engine/core/
*Анализ в процессе...*

### src/engine/db/
*Ожидает анализа...*

### src/engine/entities/
*Ожидает анализа...*

### src/engine/network/
*Ожидает анализа...*

### src/engine/olc/
*Ожидает анализа...*

### src/engine/scripting/
*Ожидает анализа...*

### src/engine/structs/
*Ожидает анализа...*

### src/engine/ui/
*Ожидает анализа...*

### src/gameplay/
*Ожидает анализа...*

---

*Последнее обновление: автоматическое при добавлении новых проблем*
