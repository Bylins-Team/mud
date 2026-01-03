# ISSUE-015: Множественные buffer overflow через sprintf в punishments.cpp

**Тип**: Buffer Overflow
**Критичность**: MEDIUM
**Файл**: `src/administration/punishments.cpp:44-61, 79-96, 114-142, 164-194, 212-240, 256-267, 283-314, 359`
**Статус**: Активна

## Описание

Модуль `punishments.cpp` содержит многочисленные вызовы небезопасной функции `sprintf()` для записи в глобальные буферы `buf` и `buf2`. Во всех функциях наказаний (SetMute, SetDumb, SetHell, SetFreeze, SetNameRoom, SetRegister, SetUnregister, SetPunisherParamsToPundata) используется один и тот же небезопасный паттерн.

```cpp
// SetMute (строки 44-61):
sprintf(buf, "Mute OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
sprintf(buf, "Mute OFF by %s", GET_NAME(ch));
sprintf(buf, "%s%s отменил$G вам молчанку.%s", kColorBoldGrn, GET_NAME(ch), kColorNrm);
sprintf(buf2, "$n2 отменили молчанку.");
sprintf(buf, "Mute ON for %s by %s(%ldh).", GET_NAME(vict), GET_NAME(ch), times);
sprintf(buf, "Mute ON (%ldh) by %s", times, GET_NAME(ch));

// SetDumb (строки 79-96):
sprintf(buf, "Dumb OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetHell (строки 114-142):
sprintf(buf, "%s removed FROM hell by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetFreeze (строки 164-194):
sprintf(buf, "Freeze OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetNameRoom (строки 212-240):
sprintf(buf, "%s removed FROM name room by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetRegister (строки 256-267):
sprintf(buf, "%s registered by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetUnregister (строки 283-314):
sprintf(buf, "%s unregistered by %s.", GET_NAME(vict), GET_NAME(ch));
// ... аналогично

// SetPunisherParamsToPundata (строка 359):
sprintf(buf, "%s : %s", ch->get_name().c_str(), reason);
```

## Воспроизведение

Теоретический сценарий эксплуатации:
1. Получить персонажа с максимально длинным допустимым именем
2. Передать очень длинный параметр `reason` в функцию наказания
3. В строке 359 функции `SetPunisherParamsToPundata()`:
   ```cpp
   sprintf(buf, "%s : %s", ch->get_name().c_str(), reason);
   ```
   При достаточно длинном `reason` произойдет переполнение буфера

## Критичность

Хотя напрямую эксплуатировать эту уязвимость сложно (имена ограничены, но `reason` может быть произвольной длины при вводе команды администратором), это представляет потенциальную угрозу, особенно в комбинации с другими багами.

## Решение

**Вариант 1**: Немедленное исправление - использовать `snprintf()`
```cpp
snprintf(buf, sizeof(buf), "Mute OFF for %s by %s.", GET_NAME(vict), GET_NAME(ch));
```

**Вариант 2**: Рефакторинг на `std::string`
```cpp
std::string message = "Mute OFF for " + std::string(GET_NAME(vict)) + " by " + std::string(GET_NAME(ch)) + ".";
mudlog(message.c_str(), DEF, std::max(kLvlImmortal, GET_INVIS_LEV(ch)), SYSLOG, true);
```

**Вариант 3**: Использовать форматирование C++20
```cpp
auto message = std::format("Mute OFF for {} by {}.", GET_NAME(vict), GET_NAME(ch));
```

## Дополнительная проблема

В строке 359 параметр `reason` передается напрямую от пользователя без валидации длины:
```cpp
sprintf(buf, "%s : %s", ch->get_name().c_str(), reason);
```

Рекомендуется добавить ограничение на длину `reason` или использовать `snprintf()`:
```cpp
snprintf(buf, sizeof(buf), "%s : %s", ch->get_name().c_str(), reason);
```
