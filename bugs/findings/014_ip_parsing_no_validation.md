# Отсутствие валидации при парсинге IP адресов

**Файл**: src/administration/proxy.cpp
**Строки**: 116-118, 126-128
**Критичность**: HIGH

## Проблема
Функции `TxtToIp()` используют `sscanf` для парсинга IP адресов без проверки корректности результата и диапазона значений:

```cpp
unsigned long TxtToIp(const char *text) {
    int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
    unsigned long ip = 0;

    sscanf(text, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);  // Нет проверки возвращаемого значения!
    ip = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;    // Может быть отрицательным или > 255!
    return ip;
}

unsigned long TxtToIp(std::string &text) {
    int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
    unsigned long ip = 0;

    sscanf(text.c_str(), "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);  // Аналогично
    ip = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
    return ip;
}
```

## Проблемы безопасности

### 1. Нет проверки успешности парсинга
`sscanf` возвращает количество успешно прочитанных элементов. Если формат строки не соответствует IP адресу, переменные ip1-ip4 могут остаться неинициализированными (хотя они инициализированы нулями).

Примеры некорректного ввода:
- `"abc.def.ghi.jkl"` → sscanf вернёт 0, ip1-ip4 останутся 0
- `"192.168"` → sscanf вернёт 2, ip3 и ip4 останутся 0
- `"192.168.1"` → sscanf вернёт 3, ip4 останется 0

### 2. Нет проверки диапазона
IP октеты должны быть в диапазоне [0, 255], но sscanf с форматом `%d` может прочитать любое целое число:

```
"300.400.500.600" → ip1=300, ip2=400, ip3=500, ip4=600
"-1.2.3.4"        → ip1=-1, ip2=2, ip3=3, ip4=4
"999999.1.2.3"    → ip1=999999, ip2=1, ip3=2, ip4=3
```

### 3. Integer overflow при сдвиге
```cpp
ip = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;
```

Если ip1 < 0, то `ip1 << 24` вызывает **undefined behavior** (сдвиг отрицательного числа).
Если ip1 > 255, результат некорректен.

## Воспроизведение
1. Создать файл `lib/misc/proxy.lst` с содержимым:
```
300.400.500.600  0  10  Test proxy
```
2. Запустить сервер
3. При загрузке proxy list будет создан некорректный IP адрес
4. CheckProxy() будет сравнивать с неправильными значениями

## Эксплуатация
Злоумышленник с доступом к файлам сервера может:
- Обойти проверку proxy, используя некорректные IP адреса
- Создать DoS, вызвав integer overflow
- Получить доступ с IP, которые должны быть заблокированы

## Решение

```cpp
unsigned long TxtToIp(const char *text) {
    int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;

    // Проверка успешности парсинга
    int parsed = sscanf(text, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
    if (parsed != 4) {
        log("SYSERR: Invalid IP address format: %s", text);
        return 0;  // или выбросить исключение
    }

    // Проверка диапазона [0, 255]
    if (ip1 < 0 || ip1 > 255 ||
        ip2 < 0 || ip2 > 255 ||
        ip3 < 0 || ip3 > 255 ||
        ip4 < 0 || ip4 > 255) {
        log("SYSERR: IP address octets out of range [0-255]: %s", text);
        return 0;
    }

    // Безопасное приведение к unsigned
    unsigned long ip = ((unsigned long)ip1 << 24) |
                       ((unsigned long)ip2 << 16) |
                       ((unsigned long)ip3 << 8) |
                       (unsigned long)ip4;
    return ip;
}
```

**Еще лучше - использовать стандартные функции:**

```cpp
#include <arpa/inet.h>  // для inet_pton

unsigned long TxtToIp(const char *text) {
    struct in_addr addr;
    if (inet_pton(AF_INET, text, &addr) != 1) {
        log("SYSERR: Invalid IP address: %s", text);
        return 0;
    }
    return ntohl(addr.s_addr);  // network byte order to host byte order
}
```

**ВАЖНО**: Исправить обе перегруженные версии функции!
