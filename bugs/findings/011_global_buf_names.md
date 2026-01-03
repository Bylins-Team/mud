# Использование глобального buf без синхронизации (names.cpp)

**Файл**: src/administration/names.cpp
**Строки**: 57, 59, 93, 95, 384, 395
**Критичность**: HIGH

## Проблема
Множественное использование глобальной переменной `buf` без объявления и без синхронизации (та же проблема, что и в ban.cpp - см. 004_global_buf_race.md):

```cpp
// Строки 57-59
sprintf(buf, "\r\n���� ��� ��������!\r\n");
iosystem::write_to_output(buf, d);
sprintf(buf, "AUTOAGREE: %s was agreed by %s", GET_PC_NAME(d->character), immname);
log(buf, d);

// Строки 93-95
sprintf(buf, "\r\n���� ��� ���������!\r\n");
iosystem::write_to_output(buf, d);
sprintf(buf, "AUTOAGREE: %s was disagreed by %s", GET_PC_NAME(d->character), immname);
log(buf, d);

// Строки 384-385
sprintf(buf, "&c%s �������%s ��� ������ %s.&n\r\n", GET_NAME(ch), GET_CH_SUF_1(ch), GET_NAME(vict));
SendMsgToGods(buf, true);

// Строки 395-396
sprintf(buf, "&c%s ��������%s ��� ������ %s.&n\r\n", GET_NAME(ch), GET_CH_SUF_1(ch), GET_NAME(vict));
SendMsgToGods(buf, true);
```

Race condition при параллельных вызовах функций (несколько игроков одновременно регистрируются или админы одновременно одобряют имена).

## Проблемы безопасности
1. **Перемешивание данных**: Если два потока выполняют `sprintf(buf, ...)` одновременно, данные смешиваются
2. **Некорректные логи**: Записи в лог могут содержать неправильную информацию
3. **Потенциальный crash**: При одновременной записи возможна порча памяти

## Решение
Использовать локальные буферы или std::string:

```cpp
// Вариант 1: Локальный буфер
char local_buf[kMaxStringLength];
sprintf(local_buf, "\r\n���� ��� ��������!\r\n");
iosystem::write_to_output(local_buf, d);
sprintf(local_buf, "AUTOAGREE: %s was agreed by %s", GET_PC_NAME(d->character), immname);
log(local_buf, d);

// Вариант 2: std::string (РЕКОМЕНДУЕТСЯ)
std::string msg = "\r\n���� ��� ��������!\r\n";
iosystem::write_to_output(msg.c_str(), d);

std::string log_msg = fmt::format("AUTOAGREE: {} was agreed by {}",
                                   GET_PC_NAME(d->character), immname);
log(log_msg.c_str(), d);

// Для строк 384-385
std::string god_msg = fmt::format("&c{} �������{} ��� ������ {}.&n\r\n",
                                   GET_NAME(ch), GET_CH_SUF_1(ch), GET_NAME(vict));
SendMsgToGods(god_msg.c_str(), true);
```

**ВАЖНО**: Та же проблема встречается и в других файлах (ban.cpp). Необходим глобальный рефакторинг для устранения использования глобального `buf`.
