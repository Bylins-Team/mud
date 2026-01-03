# ISSUE-006: Использование неинициализированного глобального буфера

**Тип**: Memory Safety / Race Condition  
**Критичность**: HIGH
**Файл**: `src/administration/ban.cpp:167, 211, 343, 358, 361, 379, 395, 421, 446, 465, 484`
**Статус**: Активна

## Описание

Код использует глобальную переменную `buf` без явного объявления в этом файле. Использование глобального буфера в многопоточном окружении создаёт race condition:

```cpp
sprintf(buf, "%s has banned %s for %s players(%s) (%dh).",
        BannerName.c_str(), BannedIp.c_str(), ban_types[BanType], ...);
mudlog(buf, BRF, kLvlGod, SYSLOG, true);
```

## Воспроизведение

1. Два администратора одновременно банят разных пользователей
2. Оба вызова используют один и тот же глобальный `buf`
3. Данные перемешиваются, логи содержат неправильную информацию
4. Возможен crash при одновременной записи

## Решение

1. Использовать локальные буферы или `std::string`
2. Использовать `fmt::format` (библиотека уже подключена)
3. Если buf действительно глобальный - добавить синхронизацию (мьютекс)

```cpp
std::string msg = fmt::format("{} has banned {} for {} players({}) ({}h).",
                               BannerName, BannedIp, ban_types[BanType], 
                               temp_node_ptr->BanReason, UnbanDate);
mudlog(msg.c_str(), BRF, kLvlGod, SYSLOG, true);
```
