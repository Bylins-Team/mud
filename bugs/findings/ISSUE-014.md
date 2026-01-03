# ISSUE-014: Множественные buffer overflow через sprintf в privilege.cpp

**Тип**: Buffer Overflow
**Критичность**: MEDIUM
**Файл**: `src/administration/privilege.cpp:44-49, 54-61`
**Статус**: Активна

## Описание

В модуле `privilege.cpp` используются глобальные буферы `buf` и `buf2` с размером `kMaxStringLength` (32768 байт), но запись в них происходит через небезопасную функцию `sprintf()` без проверки размера. При длинных именах игроков возможно переполнение буфера.

```cpp
// Примеры из функции SetMute():
sprintf(buf, "Mute OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
mudlog(buf, DEF, std::max(kLvlImmortal, GET_INVIS_LEV(ch)), SYSLOG, true);
imm_log("%s", buf);
sprintf(buf, "Mute OFF by %s", GET_NAME(ch));
AddKarma(vict, buf, reason);
sprintf(buf, "%s%s отменил$G вам молчанку.%s", kColorBoldGrn, GET_NAME(ch), kColorNrm);
sprintf(buf2, "$n2 отменили молчанку.");
```

Хотя имена игроков обычно ограничены разумной длиной, использование `sprintf()` вместо `snprintf()` является плохой практикой и потенциальным источником уязвимостей.

## Воспроизведение

Хотя прямая эксплуатация затруднена из-за ограничений на длину имени игрока, теоретически возможен сценарий:

1. Если где-то в коде есть способ установить длинное имя (через баг или административную команду)
2. Выполнение команды punishment с этим именем
3. Переполнение глобального буфера, что может повредить другие данные в памяти

## Замечание

Использование глобальных буферов `buf`, `buf1`, `buf2` само по себе является проблемой в многопоточном окружении и создает race conditions. Это устаревший паттерн из C-кода CircleMUD.

## Решение

Использовать безопасные альтернативы:

**Вариант 1**: `snprintf()` с проверкой размера
```cpp
snprintf(buf, sizeof(buf), "Mute OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
```

**Вариант 2** (предпочтительно): Использовать `std::string` и форматирование
```cpp
std::string message = fmt::format("Mute OFF for {} by {}.", GET_NAME(vict), GET_NAME(ch));
mudlog(message.c_str(), DEF, std::max(kLvlImmortal, GET_INVIS_LEV(ch)), SYSLOG, true);
```

**Вариант 3**: Использовать C++20 `std::format`
```cpp
auto message = std::format("Mute OFF for {} by {}.", GET_NAME(vict), GET_NAME(ch));
```

## Рекомендации

Это системная проблема во всем коде MUD. Рекомендуется:
1. Постепенный рефакторинг для замены глобальных буферов на локальные `std::string`
2. Использование `snprintf()` как минимальное исправление для критичных мест
3. Добавление статического анализа кода для обнаружения небезопасных функций
