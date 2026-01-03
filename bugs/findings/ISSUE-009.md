# ISSUE-009: Memory leak в ReadCharacterInvalidNamesList

**Тип**: Memory Leak
**Критичность**: LOW
**Файл**: `src/administration/names.cpp:505-524`
**Статус**: Активна

## Описание

Функция `ReadCharacterInvalidNamesList()` выделяет память через `str_dup()`, но никогда её не освобождает:

```cpp
while (get_line(fp, temp) && num_invalid < kMaxInvalidNames)
    invalid_list[num_invalid++] = str_dup(temp);  // Память выделяется
```

Память `invalid_list` никогда не освобождается при shutdown или reload.

## Воспроизведение

1. Запуск сервера - вызов `ReadCharacterInvalidNamesList()`
2. Выделение памяти для всех запрещённых имён
3. При перезагрузке списка (если такая функция есть) - повторное выделение без освобождения
4. Накопление утечек памяти

## Решение

**Вариант 1**: Использовать `std::vector<std::string>`:

```cpp
std::vector<std::string> invalid_list;

void ReadCharacterInvalidNamesList() {
    FILE *fp;
    char temp[256];

    if (!(fp = fopen(XNAME_FILE, "r"))) {
        perror("SYSERR: Unable to open '" XNAME_FILE "' for reading");
        return;
    }

    invalid_list.clear();  // Автоматически освобождает предыдущие данные
    while (get_line(fp, temp)) {
        invalid_list.push_back(temp);
    }

    fclose(fp);
}
```

**Вариант 2**: Добавить функцию очистки:

```cpp
void ClearInvalidNamesList() {
    for (size_t i = 0; i < num_invalid; i++) {
        free(invalid_list[i]);
    }
    num_invalid = 0;
}
```
