# strcpy без проверки границ в IsValidName

**Файл**: src/administration/names.cpp
**Строка**: 477
**Критичность**: MEDIUM

## Проблема
Использование `strcpy` без проверки длины исходной строки:

```cpp
int IsValidName(char *newname) {
    // ...

    // change to lowercase
    char tempname[kMaxInputLength];
    strcpy(tempname, newname);  // ОПАСНО: newname может быть длиннее kMaxInputLength
    for (std::size_t i = 0; tempname[i]; i++) {
        tempname[i] = LOWER(tempname[i]);
    }

    // ...
}
```

Параметр `newname` приходит из внешнего вызова без гарантий, что его длина <= kMaxInputLength. Если `newname` длиннее, происходит buffer overflow.

## Воспроизведение
```cpp
// Вызов с длинной строкой
char long_name[10000];
memset(long_name, 'A', 9999);
long_name[9999] = '\0';
IsValidName(long_name);  // Buffer overflow!
```

## Влияние
- Stack buffer overflow
- Возможна перезапись локальных переменных на стеке
- Потенциальное выполнение кода (если эксплуатировать через ROP/shellcode)
- Crash сервера

## Решение
**Вариант 1**: Использовать `strncpy` с проверкой:
```cpp
int IsValidName(char *newname) {
    if (!*invalid_list || num_invalid < 1) {
        return true;
    }

    // Проверка длины
    size_t len = strlen(newname);
    if (len >= kMaxInputLength) {
        return false;  // Имя слишком длинное
    }

    // change to lowercase
    char tempname[kMaxInputLength];
    strncpy(tempname, newname, kMaxInputLength - 1);
    tempname[kMaxInputLength - 1] = '\0';  // Гарантируем null-terminator

    for (std::size_t i = 0; tempname[i]; i++) {
        tempname[i] = LOWER(tempname[i]);
    }

    // ...
}
```

**Вариант 2**: Использовать std::string (РЕКОМЕНДУЕТСЯ):
```cpp
bool IsValidName(const std::string& newname) {
    if (!*invalid_list || num_invalid < 1) {
        return true;
    }

    // Проверка длины
    if (newname.length() >= kMaxInputLength) {
        return false;
    }

    // change to lowercase
    std::string tempname = newname;
    std::transform(tempname.begin(), tempname.end(), tempname.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Does the desired name contain a string in the invalid list?
    for (std::size_t i = 0; i < num_invalid; i++) {
        if (tempname.find(invalid_list[i]) != std::string::npos) {
            return false;
        }
    }

    return true;
}
```

**Рекомендация**: Перейти на std::string для всех операций с именами, чтобы избежать подобных проблем в будущем.
