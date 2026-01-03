# Логическая ошибка в DisconnectBannedIp

**Файл**: src/administration/ban.cpp
**Строки**: 549-564
**Критичность**: MEDIUM

## Проблема
```cpp
for (d = descriptor_list; d; d = d->next) {
    if (d->host == Ip) {
        if (d->state == EConState::kDisconnect || d->state == EConState::kClose)
            return;  // ОШИБКА: выход из всей функции!
        // ...
    }
}
```

При нескольких подключениях с одного IP, если первое уже в Disconnect/Close, функция выходит и остальные не обрабатываются.

## Воспроизведение
1. 3 подключения с одного IP
2. Одно уже Disconnect
3. Ban IP - остальные 2 не отключатся

## Решение
```cpp
if (d->state == EConState::kDisconnect || d->state == EConState::kClose)
    continue;  // Пропустить, но проверить остальные
```
