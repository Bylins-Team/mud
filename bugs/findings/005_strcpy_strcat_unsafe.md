# Небезопасные strcpy/strcat

**Файл**: src/administration/ban.cpp
**Строки**: 342, 356, 361-362, 378, 393, 398-399, 420
**Критичность**: MEDIUM

## Проблема
Использование strcpy/strcat без проверки размера буфера:
```cpp
strcpy(format, "%-25.25s  %-8.8s  %-10.10s  %-16.16s %-8.8s\r\n");  // 342
strcpy(buf, i->BanReason.c_str());  // 361
strcat(buf, "\r\n");  // 362
```

i->BanReason может быть длиннее buf.

## Воспроизведение
1. Создать бан с длинным BanReason
2. При отображении - buffer overflow

## Решение
Использовать std::string или snprintf с проверкой.
