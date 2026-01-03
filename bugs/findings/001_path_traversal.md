# Path Traversal в Account::save_to_file/read_from_file

**Файл**: src/administration/accounts.cpp
**Строки**: 108, 127
**Критичность**: MEDIUM

## Проблема
Email используется напрямую в пути файла:
```cpp
out.open(LIB_ACCOUNTS + this->email);  // 108
std::ifstream in(LIB_ACCOUNTS + this->email);  // 127
```

Email может содержать `../` для выхода за пределы директории.

## Воспроизведение
1. Email = `../../etc/passwd`
2. save_to_file() откроет файл вне LIB_ACCOUNTS

## Решение
```cpp
std::string safe_name = sha256(this->email);
out.open(LIB_ACCOUNTS + safe_name);
```
