# ISSUE-012: Отсутствие проверки размера массива в ReloadBan/ReloadProxyBan

**Тип**: Code Quality / Potential DoS
**Критичность**: LOW
**Файл**: `src/administration/ban.cpp:217-267, 269-303`
**Статус**: Активна

## Описание

Функции `ReloadBan()` и `ReloadProxyBan()` читают данные из файлов без ограничения количества записей:

```cpp
while (!feof(loaded)) {
    DiskIo::read_line(loaded, str_to_parse, true);
    // ... парсинг ...
    ban_list_.push_front(ptr);  // Нет ограничения на размер списка
}
```

Если файл содержит миллионы записей, произойдёт неконтролируемое потребление памяти.

## Воспроизведение

1. Злоумышленник с доступом к файловой системе (или через уязвимость) модифицирует файлы `etc/badsites` или `etc/badproxy`
2. Добавляет миллионы записей в ban-файлы
3. При перезагрузке сервера или команде reload ban
4. Сервер загружает все записи в память
5. Потребление памяти растёт до crash/OOM killer

## Решение

**Вариант 1**: Добавить ограничение на размер:

```cpp
const size_t MAX_BAN_ENTRIES = 100000;  // Разумный лимит

bool BanList::ReloadBan() {
    FILE *loaded;
    ban_list_.clear();
    if ((loaded = fopen(ban_filename, "r"))) {
        std::string str_to_parse;
        size_t count = 0;

        while (!feof(loaded) && count < MAX_BAN_ENTRIES) {
            DiskIo::read_line(loaded, str_to_parse, true);
            // ... парсинг ...
            ban_list_.push_front(ptr);
            count++;
        }
        
        if (count >= MAX_BAN_ENTRIES) {
            log("WARNING: Ban list truncated at %zu entries", MAX_BAN_ENTRIES);
        }
        
        fclose(loaded);
        return true;
    }
    // ...
}
```

**Вариант 2**: Мониторинг размера файла перед загрузкой:

```cpp
// Проверить размер файла
struct stat st;
if (stat(ban_filename, &st) == 0) {
    if (st.st_size > 10 * 1024 * 1024) {  // 10MB лимит
        log("ERROR: Ban file too large: %ld bytes", st.st_size);
        return false;
    }
}
```
