# ISSUE-003: Path Traversal в сохранении аккаунтов

**Тип**: Path Traversal
**Критичность**: MEDIUM
**Файл**: `src/administration/accounts.cpp:108, 127`
**Статус**: Активна

## Описание

Функции `save_to_file()` и `read_from_file()` открывают файлы, используя email как часть пути без валидации:

```cpp
out.open(LIB_ACCOUNTS + this->email);
std::ifstream in(LIB_ACCOUNTS + this->email);
```

Если email содержит `../`, злоумышленник может читать/писать файлы вне предполагаемой директории.

## Воспроизведение

1. Злоумышленник создает аккаунт с email: `../../etc/passwd`
2. Система попытается открыть файл вне `LIB_ACCOUNTS`
3. Возможно чтение/запись произвольных файлов в системе

## Решение

1. Валидировать email перед использованием в путях файлов
2. Проверять, что итоговый путь находится внутри `LIB_ACCOUNTS` (canonical path check)
3. Использовать whitelist допустимых символов для email
4. Рекомендуется использовать hash от email как имя файла вместо прямого использования

## Пример исправления

```cpp
#include <filesystem>
#include <openssl/sha.h>

std::string get_safe_account_filename(const std::string &email) {
    // Вариант 1: Хеш от email
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)email.c_str(), email.length(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void Account::save_to_file() {
    std::string safe_filename = get_safe_account_filename(this->email);
    std::filesystem::path account_path = std::filesystem::path(LIB_ACCOUNTS) / safe_filename;

    // Проверка canonical path
    std::filesystem::path canonical = std::filesystem::weakly_canonical(account_path);
    std::filesystem::path lib_canonical = std::filesystem::weakly_canonical(LIB_ACCOUNTS);

    if (!canonical.string().starts_with(lib_canonical.string())) {
        log("SECURITY: Path traversal attempt detected for email: %s", this->email.c_str());
        return;
    }

    std::ofstream out(canonical);
    // ... остальной код
}
```
