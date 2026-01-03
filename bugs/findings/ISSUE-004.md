# ISSUE-004: Потенциальный out-of-bounds доступ

**Тип**: Buffer Overflow / Out of Bounds  
**Критичность**: MEDIUM
**Файл**: `src/administration/accounts.cpp:82, 98`
**Статус**: Активна

## Описание

Код обращается к `name[0]` без предварительной проверки, что строка `name` не пустая:

```cpp
std::string name = GetNameByUnique(x);
name[0] = UPPER(name[0]);  // Если name пустая - UB
```

## Воспроизведение

1. Функция `GetNameByUnique()` возвращает пустую строку
2. Обращение к `name[0]` вызывает undefined behavior
3. Возможен crash или непредсказуемое поведение

## Решение

```cpp
std::string name = GetNameByUnique(x);
if (!name.empty()) {
    name[0] = UPPER(name[0]);
}
```
