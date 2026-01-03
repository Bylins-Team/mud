# Использование глобального buf без синхронизации

**Файл**: src/administration/ban.cpp
**Строки**: 167, 211, 343, 358, 361, 395, 421, 446, 465, 484
**Критичность**: HIGH

## Проблема
Множественное использование глобальной переменной `buf` без объявления в файле и без синхронизации:
```cpp
sprintf(buf, "%s has banned %s for %s players(%s) (%dh).", ...);  // 167
mudlog(buf, BRF, kLvlGod, SYSLOG, true);
```

Race condition при параллельных вызовах.

## Воспроизведение
1. Два админа одновременно банят пользователей
2. Данные в buf перемешиваются
3. Некорректные логи, возможен crash

## Решение
```cpp
std::string msg = fmt::format("{} has banned {} for {} players({}) ({}h).",
                               BannerName, BannedIp, ban_types[BanType],
                               BanReason, UnbanDate);
mudlog(msg.c_str(), BRF, kLvlGod, SYSLOG, true);
```
