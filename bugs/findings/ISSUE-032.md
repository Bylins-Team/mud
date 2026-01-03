# ISSUE-032: Синтаксическая ошибка в perform_socket_read() - незакрытый комментарий

**Файл:** `/home/kvirund/repos/mud/src/engine/core/iosystem.cpp`
**Строка:** 603
**Серьезность:** КРИТИЧЕСКАЯ
**Категория:** Compilation Error / Syntax Error
**Влияние:** Код может не компилироваться или вести себя непредсказуемо

## Описание проблемы

В функции `perform_socket_read()` обнаружена **синтаксическая ошибка** - незакрытый блочный комментарий, который может привести к тому, что часть критического кода обработки ошибок будет закомментирована.

### Проблемный код (строка 603):

```cpp
ssize_t perform_socket_read(socket_t desc, char *read_point, size_t space_left) {
	ssize_t ret;

	// ... код чтения из сокета ...

	// read() returned 0, meaning we got an EOF.
	if (ret == 0) {
		log("WARNING: EOF on socket read (connection broken by peer)");
		return (-1);
	}

	/ * read returned a value < 0: there was an error  // <-- ОШИБКА! Пробел между / и *

#if defined(CIRCLE_WINDOWS)    // Windows
	if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR)
		return (0);
#else
	// ... обработка ошибок ...
#endif

	/*
	 * We don't know what happened, cut them off.
	 */
	perror("SYSERR: perform_socket_read: about to lose connection");
	return (-1);
}
```

## Анализ проблемы

### Что не так:

1. **Строка 603:** `/ * read returned...`
   - Пробел между `/` и `*`
   - Это **НЕ** начало блочного комментария
   - Это деление на разыменование указателя!

2. **Компилятор может интерпретировать как:**
   ```cpp
   / * read  // деление на переменную "read"
   ```

3. **Возможные последствия:**
   - Код не компилируется
   - Или компилируется с предупреждениями
   - Непредсказуемое поведение обработки ошибок сокетов

## Почему это критично

1. **Нарушена обработка ошибок сокетов**
   - Весь блок обработки errno может быть пропущен
   - Ошибки чтения из сокета не обрабатываются корректно

2. **Отсутствие обработки EINTR, EAGAIN, EWOULDBLOCK**
   - Эти ошибки должны обрабатываться как "попробовать снова"
   - Без обработки соединение будет разорвано при временных ошибках

3. **Denial of Service**
   - Нормальные временные ошибки приведут к разрыву соединения
   - Игроки будут отключаться при малейших сетевых проблемах

## Как это могло произойти

Скорее всего это результат:
- Неудачного форматирования кода
- Автоматического рефакторинга
- Ошибки при слиянии веток (merge conflict)

## Правильный вариант

```cpp
ssize_t perform_socket_read(socket_t desc, char *read_point, size_t space_left) {
	ssize_t ret;

#if defined(CIRCLE_ACORN)
	ret = recv(desc, read_point, space_left, MSG_DONTWAIT);
#elif defined(CIRCLE_WINDOWS)
	ret = recv(desc, read_point, static_cast<int>(space_left), 0);
#else
	ret = read(desc, read_point, space_left);
#endif

	// Read was successful.
	if (ret > 0) {
		number_of_bytes_read += ret;
		return (ret);
	}

	// read() returned 0, meaning we got an EOF.
	if (ret == 0) {
		log("WARNING: EOF on socket read (connection broken by peer)");
		return (-1);
	}

	/* read returned a value < 0: there was an error */  // <-- ИСПРАВЛЕНО!

#if defined(CIRCLE_WINDOWS)    // Windows
	if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR)
		return (0);
#else

#ifdef EINTR            // Interrupted system call - various platforms
	if (errno == EINTR)
		return (0);
#endif

#ifdef EAGAIN            // POSIX
	if (errno == EAGAIN)
		return (0);
#endif

#ifdef EWOULDBLOCK        // BSD
	if (errno == EWOULDBLOCK)
		return (0);
#endif                // EWOULDBLOCK

#ifdef EDEADLK            // Macintosh
	if (errno == EDEADLK)
		return (0);
#endif

#endif                // CIRCLE_WINDOWS

	/*
	 * We don't know what happened, cut them off. This qualifies for
	 * a SYSERR because we have no idea what happened at this point.
	 */
	perror("SYSERR: perform_socket_read: about to lose connection");
	return (-1);
}
```

## Проверка компиляции

```bash
# Проверить, компилируется ли текущий код:
cd /home/kvirund/repos/mud/build
make 2>&1 | grep -A5 -B5 "perform_socket_read"

# Если есть ошибки/предупреждения связанные с этой функцией,
# это подтвердит проблему
```

## Исправление

### Вариант 1: Минимальное исправление
```cpp
// Строка 603: Убрать пробел между / и *
/* read returned a value < 0: there was an error */
```

### Вариант 2: Использовать однострочный комментарий
```cpp
// read returned a value < 0: there was an error
```

### Вариант 3: Удалить комментарий (код и так понятен)
```cpp
// Просто удалить строку 603
```

## Дополнительная проблема: отсутствие null-termination

После чтения из сокета нужно гарантировать null-termination:

```cpp
ssize_t perform_socket_read(socket_t desc, char *read_point, size_t space_left) {
	ssize_t ret;

	// ... чтение из сокета ...

	// Read was successful.
	if (ret > 0) {
		read_point[ret] = '\0';  // <-- ДОБАВИТЬ ЭТО!
		number_of_bytes_read += ret;
		return (ret);
	}

	// ... остальной код ...
}
```

**Примечание:** null-termination добавляется в вызывающей функции `process_input()` (строка 210), но лучше делать это сразу в `perform_socket_read()` для безопасности.

## Связанные проблемы

- ISSUE-030: Buffer overflow в get_from_q()
- ISSUE-031: Unsafe string operations в iosystem.cpp
- ISSUE-033: Отсутствие валидации данных из сокета

## Статус

- [x] Обнаружена
- [ ] Исправлена
- [ ] Протестирована

## Приоритет

**КРИТИЧЕСКИЙ - требует немедленного исправления**

Код может не компилироваться или иметь непредсказуемое поведение обработки сетевых ошибок.
