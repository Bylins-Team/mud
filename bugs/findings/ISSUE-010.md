# ISSUE-010: Некорректная логика в DisconnectBannedIp

**Тип**: Logic Error
**Критичность**: MEDIUM
**Файл**: `src/administration/ban.cpp:549-564`
**Статус**: Активна

## Описание

Функция `DisconnectBannedIp()` имеет `return` внутри проверки состояния, что приводит к выходу из функции после проверки первого дескриптора с забаненным IP:

```cpp
for (d = descriptor_list; d; d = d->next) {
    if (d->host == Ip) {
        if (d->state == EConState::kDisconnect || d->state == EConState::kClose)
            return;  // ПРОБЛЕМА: выход из функции, остальные не проверяются!
        
        iosystem::write_to_output("Your IP has been banned, disconnecting...\r\n", d);
        if (d->state == EConState::kPlaying)
            d->state = EConState::kDisconnect;
        else
            d->state = EConState::kClose;
    }
}
```

Если у одного IP несколько подключений, отключится только первое.

## Воспроизведение

1. Пользователь подключается с одного IP несколько раз (открывает 3-4 telnet сессии)
2. Один из дескрипторов уже в состоянии Disconnect/Close
3. Администратор банит IP - вызов `DisconnectBannedIp()`
4. Функция находит первый дескриптор, видит что он уже в Disconnect/Close
5. Функция делает `return` и выходит
6. Остальные 2-3 подключения с этого IP остаются активными

## Решение

```cpp
for (d = descriptor_list; d; d = d->next) {
    if (d->host == Ip) {
        if (d->state == EConState::kDisconnect || d->state == EConState::kClose)
            continue;  // Пропустить этот дескриптор, но проверить остальные
        
        iosystem::write_to_output("Your IP has been banned, disconnecting...\r\n", d);
        if (d->state == EConState::kPlaying)
            d->state = EConState::kDisconnect;
        else
            d->state = EConState::kClose;
    }
}
```
