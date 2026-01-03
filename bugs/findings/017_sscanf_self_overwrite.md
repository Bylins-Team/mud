# sscanf записывает в буфер, из которого читает

**Файл**: src/engine/db/help.cpp
**Строка**: 1492
**Критичность**: HIGH

## Проблема
В функции `do_help()` используется `sscanf`, который читает из буфера `argument` и **записывает результат в тот же буфер**:

```cpp
void do_help(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
    // ...
    sscanf(argument, "%d.%s", &user_search.topic_num, argument);
    //                                                  ^^^^^^^^
    //                                      записывает в тот же буфер!
}
```

Это создаёт **две проблемы**:

### Проблема 1: Undefined behavior
`sscanf` читает и записывает в один и тот же буфер. Согласно стандарту C, это **undefined behavior**, так как буферы перекрываются.

### Проблема 2: Buffer overflow без ограничения ширины
Формат `%s` не имеет ограничения ширины. Если часть после точки в `argument` длиннее самого `argument`, произойдёт переполнение.

## Пример работы

Предположим:
- `argument = "5.hello"`
- `sscanf(argument, "%d.%s", &topic_num, argument)`

Ожидаемое поведение:
- `topic_num = 5`
- `argument = "hello"`

Реальное поведение - **undefined**:
- Может работать "правильно" (случайно)
- Может перезаписать часть строки во время чтения
- Может переполнить буфер

## Воспроизведение
```bash
# В игре выполнить команду help с длинным запросом:
help 1.AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

Если строка после точки превысит размер буфера `argument` (обычно kMaxInputLength), произойдёт overflow.

## Решение

**Вариант 1**: Использовать временный буфер:
```cpp
void do_help(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
    char temp_arg[kMaxInputLength];
    // ...

    // Читаем во временный буфер
    if (sscanf(argument, "%d.%255s", &user_search.topic_num, temp_arg) == 2) {
        // Копируем обратно
        strncpy(argument, temp_arg, kMaxInputLength - 1);
        argument[kMaxInputLength - 1] = '\0';
    }
    // ...
}
```

**Вариант 2**: Использовать std::string (РЕКОМЕНДУЕТСЯ):
```cpp
void do_help(CharData *ch, char *argument, int/* cmd*/, int/* subcmd*/) {
    std::string arg_str(argument);
    UserSearch user_search(ch);

    user_search.level = GET_GOD_FLAG(ch, EGf::kDemigod) ? kLvlImmortal : GetRealLevel(ch);
    utils::ConvertToLow(arg_str);

    // Парсинг topic_num
    size_t dot_pos = arg_str.find('.');
    if (dot_pos != std::string::npos) {
        try {
            user_search.topic_num = std::stoi(arg_str.substr(0, dot_pos));
            arg_str = arg_str.substr(dot_pos + 1);
        } catch (...) {
            user_search.topic_num = 0;
        }
    }

    // Копируем обратно в argument для совместимости
    strncpy(argument, arg_str.c_str(), kMaxInputLength - 1);
    argument[kMaxInputLength - 1] = '\0';
    // ...
}
```

**КРИТИЧЕСКИ ВАЖНО**:
- НИКОГДА не использовать sscanf/scanf для записи в тот же буфер, из которого читаем
- Это нарушение стандарта C (undefined behavior)
- Может привести к непредсказуемым результатам и уязвимостям
