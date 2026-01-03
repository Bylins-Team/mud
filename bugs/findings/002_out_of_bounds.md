# Out-of-bounds доступ к name[0]

**Файл**: src/administration/accounts.cpp
**Строки**: 82, 98
**Критичность**: MEDIUM

## Проблема
```cpp
std::string name = GetNameByUnique(x);
name[0] = UPPER(name[0]);
```

Нет проверки что `name` не пустая.

## Воспроизведение
1. GetNameByUnique() возвращает ""
2. name[0] - undefined behavior

## Решение
```cpp
if (!name.empty()) {
    name[0] = UPPER(name[0]);
}
```
