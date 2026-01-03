# ISSUE-008: Небезопасное использование sscanf с фиксированными буферами

**Тип**: Buffer Overflow
**Критичность**: MEDIUM
**Файл**: `src/administration/names.cpp:46, 89, 129, 294`
**Статус**: Активна

## Описание

Функции `was_agree_name()`, `was_disagree_name()`, `rm_agree_name()`, `rm_disagree_name()` используют `sscanf()` для парсинга данных из файлов в буферы фиксированного размера без ограничения читаемой длины:

```cpp
char mortname[kMaxInputLength];
char immname[kMaxInputLength];
sscanf(temp, "%s %s %d", mortname, immname, &immlev);  // Нет ограничения длины!
```

Если файл содержит слишком длинные строки, произойдёт переполнение буфера.

## Воспроизведение

1. Злоумышленник с доступом к файловой системе модифицирует файлы ANAME_FILE или DNAME_FILE
2. Добавляет очень длинные имена (>kMaxInputLength символов)
3. При загрузке файла происходит buffer overflow
4. Возможно выполнение произвольного кода

## Решение

```cpp
sscanf(temp, "%255s %255s %d", mortname, immname, &immlev);  // С ограничением длины
```

Или использовать более безопасный парсинг через `std::istringstream`:

```cpp
std::istringstream iss(temp);
std::string mortname_str, immname_str;
int immlev;
iss >> mortname_str >> immname_str >> immlev;
```
