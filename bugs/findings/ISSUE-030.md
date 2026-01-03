# ISSUE-030: КРИТИЧЕСКАЯ уязвимость - Buffer Overflow в get_from_q()

**Файл:** `/home/kvirund/repos/mud/src/engine/core/iosystem.cpp`
**Строки:** 88-104
**Серьезность:** КРИТИЧЕСКАЯ
**Категория:** Buffer Overflow / Memory Safety
**CVE потенциал:** Да (Remote Code Execution)

## Описание проблемы

Функция `get_from_q()` содержит **критическую уязвимость buffer overflow** при использовании небезопасной функции `strcpy()` без проверки размера буфера назначения.

### Проблемный код:

```cpp
int get_from_q(struct TextBlocksQueue *queue, char *dest, int *aliased) {
	struct TextBlock *tmp;

	// queue empty?
	if (!queue->head)
		return (0);

	tmp = queue->head;
	strcpy(dest, queue->head->text);  // <-- КРИТИЧЕСКАЯ УЯЗВИМОСТЬ!
	*aliased = queue->head->aliased;
	queue->head = queue->head->next;

	free(tmp->text);
	free(tmp);

	return (1);
}
```

## Почему это критично

1. **Нет проверки размера буфера `dest`**
   - Функция не знает размер целевого буфера
   - `strcpy()` копирует данные до `\0` без ограничений
   - Переполнение гарантировано при длинном вводе

2. **Вектор атаки через сеть**
   - Функция вызывается из `process_input()` (строка 468)
   - Обрабатывает данные напрямую от сокета
   - Атакующий может отправить длинную строку

3. **Потенциал для RCE**
   - Buffer overflow на стеке
   - Перезапись return address
   - Возможность выполнения произвольного кода

4. **Используется глобальный буфер `buf2`**
   - В iosystem.cpp:114: `while (get_from_q(&d->input, buf2, &dummy));`
   - `buf2` определен в globals (размер MAX_STRING_LENGTH = 16384)
   - Но размер текста из очереди не ограничен!

## Пример эксплуатации

```
1. Атакующий подключается к серверу
2. Отправляет строку > 16384 байт
3. Строка попадает в input queue
4. get_from_q() копирует её в buf2
5. Buffer overflow -> перезапись стека
6. RCE если правильно подобран payload
```

## Контекст вызова

### Из process_input() (iosystem.cpp:198):
```cpp
bytes_read = perform_socket_read(t->descriptor, read_point, space_left);
// ... обработка ...
```

### Из flush_queues() (iosystem.cpp:114):
```cpp
while (get_from_q(&d->input, buf2, &dummy));  // <-- buf2 может переполниться
```

## Рекомендации по исправлению

### Вариант 1 (минимальный):
```cpp
int get_from_q(struct TextBlocksQueue *queue, char *dest, size_t dest_size, int *aliased) {
	struct TextBlock *tmp;

	if (!queue->head)
		return (0);

	tmp = queue->head;

	// Безопасное копирование с проверкой размера
	strncpy(dest, queue->head->text, dest_size - 1);
	dest[dest_size - 1] = '\0';  // Гарантируем null-terminator

	*aliased = queue->head->aliased;
	queue->head = queue->head->next;

	free(tmp->text);
	free(tmp);

	return (1);
}
```

### Вариант 2 (лучший):
```cpp
int get_from_q(struct TextBlocksQueue *queue, char *dest, size_t dest_size, int *aliased) {
	struct TextBlock *tmp;

	if (!queue->head)
		return (0);

	tmp = queue->head;

	size_t text_len = strlen(queue->head->text);
	if (text_len >= dest_size) {
		log("SYSERR: Queue text too long (%zu bytes) for buffer (%zu bytes)",
		    text_len, dest_size);
		// Обрезаем или возвращаем ошибку
		return (-1);
	}

	strcpy(dest, queue->head->text);
	*aliased = queue->head->aliased;
	queue->head = queue->head->next;

	free(tmp->text);
	free(tmp);

	return (1);
}
```

### Обновить все вызовы:
```cpp
// Было:
while (get_from_q(&d->input, buf2, &dummy));

// Стало:
while (get_from_q(&d->input, buf2, sizeof(buf2), &dummy));
```

## Связанные уязвимости

- ISSUE-031: Множественные strcpy() без проверок в iosystem.cpp
- ISSUE-032: Buffer overflow в write_to_output()
- ISSUE-033: Unsafe sprintf() в обработке сети

## Статус

- [x] Обнаружена
- [ ] Исправлена
- [ ] Протестирована

## Приоритет

**КРИТИЧЕСКИЙ - требует немедленного исправления**

Эта уязвимость позволяет удаленному атакующему выполнить произвольный код на сервере без аутентификации.
