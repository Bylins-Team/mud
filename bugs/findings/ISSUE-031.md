# ISSUE-031: Множественные небезопасные strcpy/strcat в iosystem.cpp

**Файл:** `/home/kvirund/repos/mud/src/engine/core/iosystem.cpp`
**Строки:** 96, 131, 159, 161, 473, 482, 483, 493, 495, 571, 722, 725, 729, 735, 747
**Серьезность:** ВЫСОКАЯ
**Категория:** Buffer Overflow / Unsafe String Operations
**CVE потенциал:** Да

## Описание проблемы

По всему файлу `iosystem.cpp` используются небезопасные функции `strcpy()` и `strcat()` без проверки размеров буферов. Это создает множественные векторы для buffer overflow атак.

## Критические места

### 1. write_to_output() - строки 131, 159, 161

```cpp
void write_to_output(const char *txt, DescriptorData *t) {
	// ...
	if (t->bufspace >= size) {
		strcpy(t->output + t->bufptr, txt);  // Строка 131
		// Проверка bufspace может быть недостаточна!
	}

	strcpy(t->large_outbuf->text, t->output);  // Строка 159
	t->output = t->large_outbuf->text;
	strcat(t->output, txt);  // Строка 161 - ОПАСНО!
}
```

**Проблема:**
- `bufspace` может быть рассчитан неправильно
- `strcat()` не проверяет границы
- Потенциальная перезапись памяти

### 2. process_input() - строки 473, 482-483, 493, 495

```cpp
int process_input(DescriptorData *t) {
	// ...
	if (*tmp == '!' && !(*(tmp + 1)))
		strcpy(tmp, t->last_input);  // Строка 473 - ОПАСНО!
	else if (*tmp == '!' && *(tmp + 1)) {
		// ...
		if (t->history[cnt] && utils::IsAbbr(commandln, t->history[cnt])) {
			strcpy(tmp, t->history[cnt]);  // Строка 482
			strcpy(t->last_input, tmp);    // Строка 483
		}
	} else if (*tmp == '^') {
		if (!(failed_subst = perform_subst(t, t->last_input, tmp)))
			strcpy(t->last_input, tmp);  // Строка 493
	} else {
		strcpy(t->last_input, tmp);  // Строка 495
	}
}
```

**Проблема:**
- `t->last_input` имеет фиксированный размер (MAX_INPUT_LENGTH)
- `tmp` может содержать данные из сети
- `t->history[cnt]` может быть длиннее tmp
- Нет проверок размера

### 3. perform_subst() - строка 571

```cpp
int perform_subst(DescriptorData *t, char *orig, char *subst) {
	// ...
	strcpy(subst, newsub);  // Строка 571
}
```

### 4. process_output() - строки 722, 725, 729, 735, 747

```cpp
int process_output(DescriptorData *t) {
	// ...
	strcpy(i, "\r\n");        // Строка 722
	// ...
	strcpy(i + 2, t->output); // Строка 725 - КРИТИЧНО!
	// ...
	strcat(i, "***OVERFLOW***\r\n");  // Строка 729
	// ...
	strcat(i, "\r\n");        // Строка 735, 747
}
```

**Критично:**
- Строка 725: копирует весь output буфер без проверки
- `i` - это локальный массив или указатель
- Может привести к stack overflow

## Векторы атаки

### Атака 1: Переполнение через history
```
1. Отправить команду длиной MAX_INPUT_LENGTH
2. Сохранится в history[0]
3. Отправить "!<abbr>" для вызова из истории
4. strcpy(tmp, t->history[cnt]) переполнит tmp
5. Buffer overflow
```

### Атака 2: Переполнение через last_input
```
1. Отправить длинную команду
2. Сохранится в last_input
3. Отправить "!" для повтора
4. strcpy(tmp, t->last_input) переполнит tmp
```

### Атака 3: Переполнение output buffer
```
1. Заполнить output буфер до предела
2. Вызвать process_output()
3. strcpy(i + 2, t->output) переполнит стек
```

## Анализ размеров буферов

```cpp
// Из descriptor_data.h (предположительно)
char last_input[MAX_INPUT_LENGTH];     // 256 байт?
char *history[HISTORY_SIZE];           // Указатели на строки
char small_outbuf[SMALL_BUFSIZE];      // 4096 байт?
char inbuf[MAX_RAW_INPUT_LENGTH];      // 512 байт?
```

**Проблема:** если исторические команды или сетевые данные длиннее этих буферов, происходит overflow.

## Рекомендации по исправлению

### 1. Заменить все strcpy на безопасные варианты:

```cpp
// Было:
strcpy(dest, src);

// Стало:
size_t dest_size = sizeof(dest);
if (strlen(src) >= dest_size) {
	log("SYSERR: String truncated in %s:%d", __FILE__, __LINE__);
	strncpy(dest, src, dest_size - 1);
	dest[dest_size - 1] = '\0';
} else {
	strcpy(dest, src);
}

// Или использовать snprintf:
snprintf(dest, sizeof(dest), "%s", src);
```

### 2. Заменить все strcat на безопасные варианты:

```cpp
// Было:
strcat(dest, src);

// Стало:
size_t dest_len = strlen(dest);
size_t dest_size = sizeof(dest);
if (dest_len + strlen(src) >= dest_size) {
	log("SYSERR: String concatenation overflow in %s:%d", __FILE__, __LINE__);
	strncat(dest, src, dest_size - dest_len - 1);
} else {
	strcat(dest, src);
}

// Или использовать snprintf:
snprintf(dest + strlen(dest), sizeof(dest) - strlen(dest), "%s", src);
```

### 3. Добавить макрос для безопасного копирования:

```cpp
#define SAFE_STRCPY(dest, src) do { \
	if (strlen(src) >= sizeof(dest)) { \
		log("SYSERR: String overflow prevented at %s:%d", __FILE__, __LINE__); \
		strncpy(dest, src, sizeof(dest) - 1); \
		dest[sizeof(dest) - 1] = '\0'; \
	} else { \
		strcpy(dest, src); \
	} \
} while(0)
```

### 4. Добавить проверки в process_input():

```cpp
// Проверка перед копированием из истории
if (t->history[cnt]) {
	size_t hist_len = strlen(t->history[cnt]);
	if (hist_len >= MAX_INPUT_LENGTH) {
		log("SYSERR: History entry too long (%zu bytes)", hist_len);
		// Обрезаем или отклоняем
		continue;
	}
	strcpy(tmp, t->history[cnt]);
}
```

## Проверка всех мест использования

```bash
# Найдено 13 использований strcpy/strcat в iosystem.cpp:
grep -n "strcpy\|strcat" src/engine/core/iosystem.cpp

96:	strcpy(dest, queue->head->text);
131:	strcpy(t->output + t->bufptr, txt);
159:	strcpy(t->large_outbuf->text, t->output);
161:	strcat(t->output, txt);
473:	strcpy(tmp, t->last_input);
482:	strcpy(tmp, t->history[cnt]);
483:	strcpy(t->last_input, tmp);
493:	strcpy(t->last_input, tmp);
495:	strcpy(t->last_input, tmp);
571:	strcpy(subst, newsub);
722:	strcpy(i, "\r\n");
725:	strcpy(i + 2, t->output);
729:	strcat(i, "***OVERFLOW***\r\n");
735:	strcat(i, "\r\n");
747:	strcat(i, "\r\n");
```

**Все 15 мест требуют ревью и исправления!**

## Связанные проблемы

- ISSUE-030: Buffer overflow в get_from_q()
- ISSUE-032: Buffer overflow в write_to_output()
- Аналогичные проблемы в comm.cpp и handler.cpp

## Статус

- [x] Обнаружена
- [ ] Исправлена
- [ ] Протестирована

## Приоритет

**ВЫСОКИЙ - требует исправления в ближайшее время**

Множественные векторы для эксплуатации через сетевой ввод.
