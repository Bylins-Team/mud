# Множественные sscanf без ограничения ширины

**Файл**: src/engine/boot/boot_data_files.cpp
**Строки**: 204, 297, 431, 632, 674, 692, 711, 1027, 1071, 1494, 1625, 1627, 1696 (и возможно больше)
**Критичность**: HIGH

## Проблема
В файле обнаружено **более 10 использований** `sscanf` с форматом `%s` без ограничения ширины, что может привести к buffer overflow при парсинге файлов зон.

### Примеры:

**Строка 204** (наиболее критичная):
```cpp
void DiscreteFile::dg_read_trigger(void *proto, int type, int proto_vnum) {
    char line[kMaxTrglineLength];
    char junk[8];  // ВСЕГО 8 БАЙТ!
    int vnum, rnum, count;

    get_line(file(), line);
    count = sscanf(line, "%s %d", junk, &vnum);  // BUFFER OVERFLOW! Нет ограничения ширины
    // ...
}
```

Если первое слово в `line` длиннее 7 символов (8 байт включая null terminator), произойдёт переполнение `junk`.

**Строка 297**:
```cpp
k = sscanf(line, "%d %s %d %d", &attach_type, flags, &t, &add_flag);
```
Буфер `flags` может переполниться.

**Строка 632**:
```cpp
int parsed_entries = sscanf(m_line, " %s %d %d %d", f0, t + 1, t + 2, t + 3);
```

**Строки 674, 692, 711, 1027, 1071** - аналогичные проблемы с другими буферами.

## Воспроизведение
1. Создать файл зоны (например, `world/trg/100.trg`) с длинной строкой:
```
T AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA 1234
```

2. При загрузке сервера на строке 204 произойдёт переполнение `junk[8]`
3. Stack overflow → segfault или возможная эксплуатация

## Эксплуатация
Злоумышленник с доступом к файлам зон может:
- Вызвать переполнение стека при загрузке зоны
- Потенциально выполнить произвольный код (через ROP)
- Создать DoS (сервер упадёт при старте)

## Решение

**Для строки 204**:
```cpp
void DiscreteFile::dg_read_trigger(void *proto, int type, int proto_vnum) {
    char line[kMaxTrglineLength];
    char junk[32];  // Увеличить размер ИЛИ добавить ограничение ширины
    int vnum, rnum, count;

    get_line(file(), line);
    count = sscanf(line, "%7s %d", junk, &vnum);  // Ограничение: max 7 символов + null

    if (count != 2) {
        log("SYSERR: Error assigning trigger!");
        return;
    }
    // ...
}
```

**Универсальное решение для всех случаев**:
1. Всегда указывать ширину при `%s` в sscanf:
   ```cpp
   char buffer[32];
   sscanf(line, "%31s", buffer);  // max 31 символ + null terminator
   ```

2. Или использовать `std::istringstream`:
   ```cpp
   std::istringstream iss(line);
   std::string junk_str;
   int vnum;
   if (!(iss >> junk_str >> vnum)) {
       log("SYSERR: Error parsing trigger line");
       return;
   }
   // Проверка длины после чтения:
   if (junk_str.length() > 7) {
       log("SYSERR: Trigger keyword too long: %s", junk_str.c_str());
       return;
   }
   ```

3. Использовать `snprintf` для формирования строк формата динамически:
   ```cpp
   char format[64];
   snprintf(format, sizeof(format), "%%%ds %%d", (int)(sizeof(junk) - 1));
   count = sscanf(line, format, junk, &vnum);
   ```

**КРИТИЧЕСКИ ВАЖНО**:
- Исправить **ВСЕ** вхождения sscanf с %s в этом файле
- Провести аудит всех файлов проекта на наличие аналогичных проблем
- Это типичная уязвимость, которая может привести к RCE

**Автоматизированная проверка**:
```bash
# Найти все sscanf с %s без ширины
grep -rn 'sscanf.*%s[^0-9]' src/
```
