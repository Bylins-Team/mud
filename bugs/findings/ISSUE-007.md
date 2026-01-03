# ISSUE-007: Потенциальное переполнение буфера в strcpy

**Тип**: Buffer Overflow
**Критичность**: MEDIUM  
**Файл**: `src/administration/names.cpp:477`
**Статус**: Активна

## Описание

Функция `IsValidName()` использует `strcpy()` без проверки длины:

```cpp
char tempname[kMaxInputLength];
strcpy(tempname, newname);  // newname может быть длиннее kMaxInputLength
```

## Воспроизведение

1. Злоумышленник передаёт очень длинное имя
2. `strcpy()` копирует данные за пределы буфера `tempname`
3. Происходит buffer overflow
4. Возможна запись в соседние области стека

## Решение

```cpp
strncpy(tempname, newname, kMaxInputLength - 1);
tempname[kMaxInputLength - 1] = '\0';
```

Или лучше использовать `std::string`.
