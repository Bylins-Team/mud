# Разыменование неинициализированного итератора

**Файл**: src/administration/privilege.cpp
**Строки**: 197, 200, 203
**Критичность**: HIGH

## Проблема
В функции `parse_command_line()` используется итератор `tmp_tok_iter` без предварительной инициализации:

```cpp
void parse_command_line(const std::string &commands, int other_flags) {
    std::vector<std::string>::iterator tok_iter, tmp_tok_iter;  // tmp_tok_iter не инициализирован!
    // ...

    for (tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
        if ((*tok_iter) == "(") {
            if ((*tmp_tok_iter) == "set") {  // Строка 197 - ОПАСНО! tmp_tok_iter не инициализирован
                fill_mode = 1;
                continue;
            } else if ((*tmp_tok_iter) == "show") {  // Строка 200
                fill_mode = 2;
                continue;
            } else if ((*tmp_tok_iter) == "groups") {  // Строка 203
                fill_mode = 3;
                continue;
            }
        }
        // ...
        tmp_tok_iter = tok_iter;  // Строка 213 - ТОЛЬКО здесь инициализируется
    }
}
```

На первой итерации цикла, когда встречается `"("`, происходит разыменование `tmp_tok_iter`, который еще не был инициализирован. Это **undefined behavior**.

## Воспроизведение
Файл privilege.lst с содержимым:
```
<groups>
default = (set something)
</groups>
```

При парсинге строки `(set something)`:
1. Первый токен `"("` → проверяется `(*tmp_tok_iter) == "set"`
2. tmp_tok_iter не инициализирован → разыменование случайной памяти
3. Segmentation fault или непредсказуемое поведение

## Влияние
- **Segmentation fault** при запуске сервера
- **Непредсказуемое поведение** парсера
- Возможное **переполнение буфера** если итератор указывает на некорректную память
- **Отказ в обслуживании** (server crash)

## Решение
**Инициализировать tmp_tok_iter перед использованием:**

```cpp
void parse_command_line(const std::string &commands, int other_flags) {
    std::vector<std::string>::iterator tok_iter, tmp_tok_iter;
    std::stringstream ss;
    int fill_mode = 0;
    auto tokens = tokenize(commands);

    if (tokens.begin() == tokens.end())
        return;

    tmp_tok_iter = tokens.begin();  // ИНИЦИАЛИЗАЦИЯ ПЕРЕД ЦИКЛОМ

    for (tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
        if ((*tok_iter) == "(") {
            if (tmp_tok_iter != tokens.end() && (*tmp_tok_iter) == "set") {
                fill_mode = 1;
                continue;
            } else if (tmp_tok_iter != tokens.end() && (*tmp_tok_iter) == "show") {
                fill_mode = 2;
                continue;
            } else if (tmp_tok_iter != tokens.end() && (*tmp_tok_iter) == "groups") {
                fill_mode = 3;
                continue;
            }
        } else if ((*tok_iter) == ")") {
            fill_mode = 0;
            continue;
        }
        parse_flags(*tok_iter);
        insert_command(*tok_iter, fill_mode, other_flags);
        tmp_tok_iter = tok_iter;
    }
}
```

**Еще лучше - переписать логику:**

```cpp
void parse_command_line(const std::string &commands, int other_flags) {
    std::stringstream ss;
    int fill_mode = 0;
    auto tokens = tokenize(commands);

    if (tokens.empty())
        return;

    std::string prev_token;

    for (const auto& token : tokens) {
        if (token == "(") {
            if (prev_token == "set") {
                fill_mode = 1;
                continue;
            } else if (prev_token == "show") {
                fill_mode = 2;
                continue;
            } else if (prev_token == "groups") {
                fill_mode = 3;
                continue;
            }
        } else if (token == ")") {
            fill_mode = 0;
            continue;
        }
        parse_flags(token);
        insert_command(token, fill_mode, other_flags);
        prev_token = token;
    }
}
```

**КРИТИЧЕСКИ ВАЖНО**: Это баг может приводить к падению сервера при запуске!
