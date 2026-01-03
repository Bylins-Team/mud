# sscanf без ограничения ширины в player_index.cpp

**Файл**: src/engine/db/player_index.cpp
**Строка**: 324
**Критичность**: MEDIUM

## Проблема
В функции парсинга списка игроков используется `sscanf` с форматом `%s` без ограничения ширины:

```cpp
void SomeFunction() {  // Определить точное имя функции
    char name[kMaxInputLength], playername[kMaxInputLength];
    // ...

    while (get_line(players, name)) {
        if (!*name || *name == ';')
            continue;
        if (sscanf(name, "%s ", playername) == 0)  // BUFFER OVERFLOW RISK!
            continue;

        if (!player_table.IsPlayerExists(playername)) {
            ActualizePlayersIndex(playername);
        }
    }
}
```

Буфер `playername` имеет размер `kMaxInputLength`, но `sscanf` может записать туда строку любой длины из `name`.

## Воспроизведение
1. Создать файл списка игроков (предположительно `lib/plrs/players.lst` или аналогичный) с длинным именем:
```
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

2. Запустить сервер
3. При загрузке списка игроков на строке 324 произойдёт переполнение `playername[kMaxInputLength]`
4. Stack buffer overflow → потенциальный segfault

## Влияние
- Stack buffer overflow
- Перезапись локальных переменных
- Потенциальный crash сервера при старте
- Менее критично, чем другие sscanf проблемы, так как файл players.lst обычно контролируется сервером

## Решение

**Вариант 1**: Добавить ограничение ширины:
```cpp
// Предположим kMaxInputLength = 256
const int MAX_NAME_LEN = kMaxInputLength - 1;  // 255 символов + null terminator

while (get_line(players, name)) {
    if (!*name || *name == ';')
        continue;

    // Ограничение ширины: kMaxInputLength - 1
    char format[32];
    snprintf(format, sizeof(format), "%%%ds ", MAX_NAME_LEN);

    if (sscanf(name, format, playername) == 0)
        continue;

    if (!player_table.IsPlayerExists(playername)) {
        ActualizePlayersIndex(playername);
    }
}
```

**Вариант 2**: Использовать std::string (РЕКОМЕНДУЕТСЯ):
```cpp
std::ifstream players_file("players.lst");
std::string line;

while (std::getline(players_file, line)) {
    if (line.empty() || line[0] == ';')
        continue;

    std::istringstream iss(line);
    std::string playername;

    if (!(iss >> playername))
        continue;

    // Проверка длины
    if (playername.length() >= kMaxInputLength) {
        log("SYSERR: Player name too long: %s", playername.c_str());
        continue;
    }

    if (!player_table.IsPlayerExists(playername.c_str())) {
        ActualizePlayersIndex(playername.c_str());
    }
}
```

**Рекомендация**: Использовать вариант 2 для полного устранения проблемы.
