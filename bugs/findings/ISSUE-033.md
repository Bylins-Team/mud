# ISSUE-033: Отсутствие валидации границ массива в обработке Telnet IAC

**Файл:** `/home/kvirund/repos/mud/src/engine/core/iosystem.cpp`
**Функция:** `process_input()`
**Строки:** 213-298
**Серьезность:** ВЫСОКАЯ
**Категория:** Buffer Overread / Out-of-bounds Access
**CVE потенциал:** Да (Information Disclosure, DoS)

## Описание проблемы

В функции `process_input()` при обработке Telnet IAC (Interpret As Command) последовательностей отсутствует проверка границ буфера перед доступом к `ptr[1]` и `ptr[2]`. Это может привести к чтению за пределами выделенной памяти.

### Проблемный код:

```cpp
int process_input(DescriptorData *t) {
	// ...
	read_point[bytes_read] = '\0';    // terminate the string

	// Search for an "Interpret As Command" marker.
	for (ptr = read_point; *ptr; ptr++) {
		if (ptr[0] != (char) IAC) {
			continue;
		}

		if (ptr[1] == (char) IAC) {  // <-- НЕТ ПРОВЕРКИ: ptr + 1 в пределах буфера?
			++ptr;
		} else if (ptr[1] == (char) DO) {  // <-- НЕТ ПРОВЕРКИ
			switch (ptr[2]) {  // <-- НЕТ ПРОВЕРКИ: ptr + 2 в пределах буфера?
				case TELOPT_COMPRESS:
					// ...
			}
			memmove(ptr, ptr + 3, bytes_read - (ptr - read_point) - 3 + 1);
			// ...
		} else if (ptr[1] == (char) DONT) {
			switch (ptr[2]) {  // <-- НЕТ ПРОВЕРКИ
				// ...
			}
			memmove(ptr, ptr + 3, bytes_read - (ptr - read_point) - 3 + 1);
		} else if (ptr[1] == char(SB)) {
			switch (ptr[2]) {  // <-- НЕТ ПРОВЕРКИ
				// ...
			}
		}
	}
}
```

## Почему это опасно

### 1. Out-of-bounds Read

Если IAC байт находится в конце буфера:
```
Buffer: [...data...][IAC][\0]
                      ^     ^
                     ptr   ptr+1 - ЗДЕСЬ \0, но читаем как будто это команда!
```

Проблема:
- `ptr[1]` может быть за пределами выделенного буфера
- `ptr[2]` может быть еще дальше
- Чтение неинициализированной памяти
- Информационная утечка

### 2. Неправильная обработка усеченных команд

Telnet команды имеют формат:
- `IAC DO <option>` - 3 байта
- `IAC DONT <option>` - 3 байта
- `IAC SB <option> ... IAC SE` - переменная длина

Если пакет разделен на границе:
```
Пакет 1: [...data...][IAC]
Пакет 2: [DO][COMPRESS][...rest...]
```

Текущий код прочитает `ptr[1]` и `ptr[2]` из **следующего** пакета или неинициализированной памяти!

### 3. memmove с неправильным размером

```cpp
memmove(ptr, ptr + 3, bytes_read - (ptr - read_point) - 3 + 1);
```

Если `ptr` близко к концу буфера, расчет может дать:
- Отрицательное значение (underflow в size_t -> огромное число)
- Копирование огромного количества данных
- Перезапись памяти за пределами буфера

## Векторы атаки

### Атака 1: Вызвать чтение за пределами буфера

```python
# Отправить IAC в конце пакета
payload = b"normal command\xFF"  # \xFF = IAC
send(payload)
# Сервер прочитает ptr[1] за пределами буфера
```

### Атака 2: Вызвать memmove с некорректным размером

```python
# Отправить IAC DO близко к концу буфера
payload = b"A" * (MAX_RAW_INPUT_LENGTH - 10) + b"\xFF\xFD\x55"
# IAC DO COMPRESS в конце буфера
# memmove попытается скопировать отрицательное количество байт
```

### Атака 3: Information Disclosure

```python
# Отправить IAC в конце пакета несколько раз
# Прочитать ответ сервера
# Может содержать данные из неинициализированной памяти
```

## Дополнительная проблема: Integer Overflow в memmove

```cpp
bytes_read -= 3;  // Может стать отрицательным!
```

Если `bytes_read < 3`, после вычитания:
- `bytes_read` (ssize_t) становится отрицательным
- При использовании в size_t контексте -> огромное значение
- Переполнение буфера

## Рекомендации по исправлению

### Вариант 1: Добавить проверки границ

```cpp
int process_input(DescriptorData *t) {
	// ...
	read_point[bytes_read] = '\0';

	// Search for an "Interpret As Command" marker.
	for (ptr = read_point; *ptr; ptr++) {
		if (ptr[0] != (char) IAC) {
			continue;
		}

		// Проверка: достаточно ли байт для чтения команды?
		size_t remaining = bytes_read - (ptr - read_point);
		if (remaining < 2) {
			// Недостаточно данных для полной команды
			// Сохраним IAC для следующего чтения
			break;
		}

		if (ptr[1] == (char) IAC) {
			++ptr;
		} else if (ptr[1] == (char) DO || ptr[1] == (char) DONT) {
			if (remaining < 3) {
				// Недостаточно данных для DO/DONT команды
				break;
			}

			// Безопасная обработка
			switch (ptr[2]) {
				case TELOPT_COMPRESS:
					// ...
			}

			// Проверка перед memmove
			ssize_t move_size = bytes_read - (ptr - read_point) - 3;
			if (move_size >= 0) {
				memmove(ptr, ptr + 3, move_size + 1);
				bytes_read -= 3;
				--ptr;
			} else {
				log("SYSERR: Invalid memmove size in process_input: %zd", move_size);
				return -1;
			}
		} else if (ptr[1] == char(SB)) {
			if (remaining < 3) {
				break;
			}
			// ... обработка SB
		}
	}
}
```

### Вариант 2: Использовать более безопасный подход

```cpp
int process_input(DescriptorData *t) {
	// ...

	// Обработка IAC последовательностей
	char *read_end = read_point + bytes_read;
	for (ptr = read_point; ptr < read_end; ptr++) {
		if (ptr[0] != (char) IAC) {
			continue;
		}

		// Безопасная проверка доступности следующих байт
		if (ptr + 1 >= read_end) {
			// IAC в конце буфера, оставим для следующего чтения
			break;
		}

		if (ptr[1] == (char) IAC) {
			++ptr;
		} else if (ptr[1] == (char) DO || ptr[1] == (char) DONT) {
			if (ptr + 2 >= read_end) {
				// Неполная команда
				break;
			}
			// Безопасная обработка
			// ...
		}
	}
}
```

### Вариант 3: Буферизация неполных команд

```cpp
// В DescriptorData добавить:
struct DescriptorData {
	// ...
	char pending_iac[10];  // Буфер для неполных IAC команд
	size_t pending_iac_len;
};

// В process_input():
// 1. Проверить pending_iac, если есть - добавить в начало
// 2. Обработать IAC команды
// 3. Если команда неполная - сохранить в pending_iac
```

## Проверка на наличие проблемы

```bash
# Тест 1: Отправить IAC в конце пакета
echo -en "test\xFF" | nc localhost 4000

# Тест 2: Отправить неполную IAC команду
echo -en "\xFF\xFD" | nc localhost 4000

# Тест 3: Valgrind для поиска out-of-bounds доступа
valgrind --track-origins=yes ./circle 4000
# Затем подключиться и отправить IAC последовательности
```

## Связанные проблемы

- ISSUE-030: Buffer overflow в get_from_q()
- ISSUE-031: Unsafe string operations
- ISSUE-032: Синтаксическая ошибка в perform_socket_read()
- ISSUE-034: Integer overflow в memmove (новая проблема)

## Дополнительные риски

### 1. MSDP обработка (строка 286)

```cpp
sb_length = msdp::handle_conversation(t, ptr, bytes_read - (ptr - read_point));
```

Не проверяется:
- Достаточно ли байт для MSDP последовательности
- Корректность возвращаемого sb_length
- Может ли memmove с sb_length выйти за границы

### 2. Отсутствие санитизации bytes_read

```cpp
bytes_read -= static_cast<int>(sb_length);
```

Если `sb_length > bytes_read`:
- Отрицательное значение bytes_read
- Следующая итерация цикла с некорректным размером

## Статус

- [x] Обнаружена
- [ ] Исправлена
- [ ] Протестирована

## Приоритет

**ВЫСОКИЙ - требует исправления в ближайшее время**

Возможность DoS и информационной утечки через crafted Telnet пакеты.
