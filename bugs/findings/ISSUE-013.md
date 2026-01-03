# ISSUE-013: Использование неинициализированного итератора

**Тип**: Use-after-free / Undefined Behavior
**Критичность**: HIGH
**Файл**: `src/administration/privilege.cpp:197, 213`
**Статус**: Активна

## Описание

В функции `parse_command_line()` используется итератор `tmp_tok_iter`, который может быть неинициализирован при первом обращении. На первой итерации цикла (строки 196-214) происходит разыменование `(*tmp_tok_iter)` на строках 197 и 200, но `tmp_tok_iter` инициализируется только в конце тела цикла (строка 213).

```cpp
void parse_command_line(const std::string &commands, int other_flags) {
	std::vector<std::string>::iterator tok_iter, tmp_tok_iter;  // tmp_tok_iter не инициализирован
	// ...
	for (tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		if ((*tok_iter) == "(") {
			if ((*tmp_tok_iter) == "set") {  // ОШИБКА: разыменование неинициализированного итератора
				fill_mode = 1;
				continue;
			} else if ((*tmp_tok_iter) == "show") {  // ОШИБКА: то же самое
				fill_mode = 2;
				continue;
			}
			// ...
		}
		// ...
		tmp_tok_iter = tok_iter;  // Только здесь присваивается значение
	}
}
```

## Воспроизведение

1. В файле `privilege.lst` создать строку, начинающуюся с открывающей скобки:
   ```
   test 12345 ( set name )
   ```
2. При парсинге этой строки произойдет разыменование неинициализированного итератора
3. Undefined behavior, возможен segfault или чтение случайной памяти

## Решение

Инициализировать `tmp_tok_iter` перед циклом или добавить проверку перед использованием:

```cpp
void parse_command_line(const std::string &commands, int other_flags) {
	std::vector<std::string>::iterator tok_iter, tmp_tok_iter;
	std::stringstream ss;
	int fill_mode = 0;
	auto tokens = tokenize(commands);

	if (tokens.begin() == tokens.end())
		return;

	// Вариант 1: инициализация начальным значением
	tmp_tok_iter = tokens.begin();

	for (tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		if ((*tok_iter) == "(") {
			// Вариант 2: проверка на первую итерацию
			if (tok_iter != tokens.begin()) {
				if ((*tmp_tok_iter) == "set") {
					fill_mode = 1;
					continue;
				} else if ((*tmp_tok_iter) == "show") {
					fill_mode = 2;
					continue;
				} else if ((*tmp_tok_iter) == "groups") {
					fill_mode = 3;
					continue;
				}
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
