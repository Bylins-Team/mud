# Индекс всех найденных уязвимостей

## Обзор по критичности

### CRITICAL (0 в модуле administration)
- См. ISSUE-001 до ISSUE-012 (предыдущий анализ)

### HIGH (1)
- [ISSUE-013](ISSUE-013.md) - Использование неинициализированного итератора (privilege.cpp)

### MEDIUM (3)
- [ISSUE-014](ISSUE-014.md) - Buffer overflow через sprintf (privilege.cpp)
- [ISSUE-015](ISSUE-015.md) - Buffer overflow через sprintf (punishments.cpp)
- [ISSUE-016](ISSUE-016.md) - Неправильный порядок аргументов sprintf (punishments.cpp)

### LOW (1)
- [ISSUE-017](ISSUE-017.md) - Слабая инициализация PRNG (name_adviser.cpp)

## Обзор по файлам

### privilege.cpp
- [ISSUE-013](ISSUE-013.md) - HIGH - Использование неинициализированного итератора
- [ISSUE-014](ISSUE-014.md) - MEDIUM - Buffer overflow через sprintf

### punishments.cpp
- [ISSUE-015](ISSUE-015.md) - MEDIUM - Buffer overflow через sprintf
- [ISSUE-016](ISSUE-016.md) - MEDIUM - Неправильный порядок аргументов

### name_adviser.cpp
- [ISSUE-017](ISSUE-017.md) - LOW - Слабая инициализация PRNG

### shutdown_parameters.cpp
- Проблем не найдено ✓

## Обзор по типам уязвимостей

### Buffer Overflow
- [ISSUE-014](ISSUE-014.md) - privilege.cpp
- [ISSUE-015](ISSUE-015.md) - punishments.cpp

### Undefined Behavior
- [ISSUE-013](ISSUE-013.md) - privilege.cpp (неинициализированный итератор)

### Format String
- [ISSUE-016](ISSUE-016.md) - punishments.cpp (неправильный порядок аргументов)

### Weak Crypto/Random
- [ISSUE-017](ISSUE-017.md) - name_adviser.cpp (слабый PRNG)

## Сводные отчеты

- [SUMMARY-ADMINISTRATION.md](SUMMARY-ADMINISTRATION.md) - Полный отчет по модулю administration

## Статистика

**Всего уязвимостей**: 5 (в модуле administration)
- CRITICAL: 0
- HIGH: 1
- MEDIUM: 3
- LOW: 1

**Файлов проанализировано**: 4
**Файлов с проблемами**: 3
**Безопасных файлов**: 1
