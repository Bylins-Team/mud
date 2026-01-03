# Buffer overflow в InitZoneTypes через sscanf

**Файл**: src/engine/db/db.cpp
**Строки**: 470, 511
**Критичность**: HIGH

## Проблема
В функции `InitZoneTypes()` используется `sscanf` с форматом `%s` без ограничения ширины для чтения данных из файла `ztypes.lst`:

```cpp
void InitZoneTypes() {
    FILE *zt_file;
    char tmp[1024], dummy[128], name[128], itype_num[128];  // Строка 446
    // ...

    while (get_line(zt_file, tmp)) {
        if (!strn_cmp(tmp, "���", 3)) {
            if (sscanf(tmp, "%s %s", dummy, name) != 2) {  // Строка 470 - BUFFER OVERFLOW!
                log("Corrupted file : ztypes.lst");
                return;
            }
            // ...
        }
    }
    // ...

    while (get_line(zt_file, tmp)) {
        sscanf(tmp, "%s %s", dummy, name);  // Строка 511 - BUFFER OVERFLOW!
        for (j = 0; name[j] != '\0'; j++) {
            if (name[j] == '_') {
                name[j] = ' ';
            }
        }
        // ...
    }
}
```

Буферы `dummy` и `name` имеют размер **128 байт**, но `sscanf` может записать туда строку любой длины из файла `ztypes.lst`.

## Воспроизведение
1. Создать файл `lib/misc/ztypes.lst` с длинным словом:
```
��� AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
���� 1
```

2. Запустить сервер
3. При загрузке на строке 470 или 511 произойдёт переполнение буфера `name[128]`
4. Stack buffer overflow → segfault или RCE

## Эксплуатация
Злоумышленник с доступом к файлу `lib/misc/ztypes.lst` может:
- Вызвать переполнение стека (stack overflow)
- Перезаписать локальные переменные функции
- Потенциально выполнить произвольный код через ROP-chain
- Создать DoS (сервер упадёт при запуске)

## Решение

**Вариант 1**: Добавить ограничение ширины в sscanf:
```cpp
void InitZoneTypes() {
    FILE *zt_file;
    char tmp[1024], dummy[128], name[128], itype_num[128];
    // ...

    while (get_line(zt_file, tmp)) {
        if (!strn_cmp(tmp, "���", 3)) {
            // Ограничение: max 127 символов + null terminator
            if (sscanf(tmp, "%127s %127s", dummy, name) != 2) {
                log("Corrupted file : ztypes.lst");
                return;
            }
            // ...
        }
    }
    // ...

    while (get_line(zt_file, tmp)) {
        sscanf(tmp, "%127s %127s", dummy, name);  // Исправлено
        // ...
    }
}
```

**Вариант 2**: Использовать std::string (РЕКОМЕНДУЕТСЯ):
```cpp
void InitZoneTypes() {
    std::ifstream zt_file(LIB_MISC "ztypes.lst");
    if (!zt_file.is_open()) {
        log("Can not open ztypes.lst");
        return;
    }

    std::string line;
    while (std::getline(zt_file, line)) {
        if (line.substr(0, 3) == "���") {
            std::istringstream iss(line);
            std::string dummy, name;

            if (!(iss >> dummy >> name)) {
                log("Corrupted file : ztypes.lst");
                return;
            }

            // Проверка длины после чтения
            if (name.length() >= 128) {
                log("SYSERR: Zone type name too long: %s", name.c_str());
                return;
            }
            // ...
        }
    }
}
```

**КРИТИЧЕСКИ ВАЖНО**: Исправить обе строки (470 и 511)!
