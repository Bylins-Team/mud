# Buffer Overflow в sscanf без ограничения длины

**Файл**: src/administration/names.cpp
**Строки**: 46-47, 89, 129-130, 294
**Критичность**: HIGH

## Проблема
Множественное использование `sscanf` с форматом `%s` без ограничения ширины может вызвать buffer overflow:

```cpp
// Строки 46-47
char immname[kMaxInputLength];
char mortname[6][kMaxInputLength];
sscanf(temp, "%s %s %s %s %s %s %d %s %d", mortname[0],
       mortname[1], mortname[2], mortname[3], mortname[4], mortname[5], &sex, immname, &immlev);
//            ^^^ нет ограничения длины!

// Строка 89
char mortname[kMaxInputLength];
char immname[kMaxInputLength];
sscanf(temp, "%s %s %d", mortname, immname, &immlev);
//            ^^^ buffer overflow!

// Строки 129-130
sscanf(temp, "%s %s %s %s %s %s %d %s %d", mortname[0],
       mortname[1], mortname[2], mortname[3], mortname[4], mortname[5], &sex, immname, &immlev);

// Строка 294
sscanf(temp, "%s %s %d", mortname, immname, &immlev);
```

Если входной файл (ANAME_FILE, DNAME_FILE) содержит строки длиннее kMaxInputLength, происходит переполнение буфера.

## Воспроизведение
1. Создать файл ANAME_FILE с длинным именем:
```bash
python3 -c "print('A' * 10000 + ' B C D E F 1 admin 34')" > lib/misc/aname.lst
```
2. Запустить сервер
3. При загрузке файла произойдёт buffer overflow в mortname[0]

## Эксплуатация
Злоумышленник с доступом к файлам сервера может:
- Перезаписать стек (stack overflow)
- Выполнить произвольный код (если ASLR отключён)
- Вызвать segfault и отказ в обслуживании

## Решение
**Использовать ограничение ширины в sscanf:**

```cpp
// Вариант 1: С ограничением ширины (требует макрос)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define MAX_LEN_STR TOSTRING(kMaxInputLength)

sscanf(temp, "%" MAX_LEN_STR "s %" MAX_LEN_STR "s %" MAX_LEN_STR "s %" MAX_LEN_STR "s %"
             MAX_LEN_STR "s %" MAX_LEN_STR "s %d %" MAX_LEN_STR "s %d",
       mortname[0], mortname[1], mortname[2], mortname[3], mortname[4], mortname[5],
       &sex, immname, &immlev);
```

**Вариант 2: Использовать std::string и современный C++ (РЕКОМЕНДУЕТСЯ):**

```cpp
std::istringstream iss(temp);
std::string mortname[6];
std::string immname;
int sex, immlev;

iss >> mortname[0] >> mortname[1] >> mortname[2] >> mortname[3]
    >> mortname[4] >> mortname[5] >> sex >> immname >> immlev;

// Проверка длины после чтения
for (int i = 0; i < 6; i++) {
    if (mortname[i].length() > kMaxInputLength) {
        log("SYSERR: Name too long in agree file: %s", mortname[i].c_str());
        continue;  // Skip this entry
    }
}
```

**Вариант 3: Использовать fscanf с ограничением:**

```cpp
char format_str[100];
snprintf(format_str, sizeof(format_str),
         "%%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%d %%%ds %%d",
         kMaxInputLength-1, kMaxInputLength-1, kMaxInputLength-1,
         kMaxInputLength-1, kMaxInputLength-1, kMaxInputLength-1,
         kMaxInputLength-1);
sscanf(temp, format_str, mortname[0], mortname[1], ...);
```

**КРИТИЧЕСКИ ВАЖНО**: Исправить все 4 места использования sscanf в этом файле!
