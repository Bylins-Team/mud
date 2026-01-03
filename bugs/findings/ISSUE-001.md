# ISSUE-001: Command Injection в функциях отправки паролей

**Тип**: Command Injection
**Критичность**: CRITICAL
**Файл**: `src/administration/password.cpp:65-75`
**Статус**: Активна

## Описание

Функции `send_password()` формируют командную строку для `system()`, напрямую подставляя пользовательские данные (email, password, name) без какой-либо санитизации или экранирования. Это классическая уязвимость command injection.

```cpp
void send_password(std::string email, std::string password, std::string name) {
    std::string cmd_line = "python3 change_pass.py " + email + " " + password + " " + name + " &";
    auto result = system(cmd_line.c_str());
    UNUSED_ARG(result);
}
```

## Воспроизведение

1. Злоумышленник регистрирует аккаунт с email вида: `user@test.com; rm -rf /`
2. При восстановлении пароля вызывается `send_password()`
3. Выполняется команда: `python3 change_pass.py user@test.com; rm -rf / password123 name &`
4. Происходит выполнение произвольной команды в системе

## Решение

1. **Немедленно**: Использовать параметризованный вызов вместо `system()`:
   - Использовать `execvp()` или `posix_spawn()` с массивом аргументов
   - Или использовать библиотеку для безопасного вызова процессов
2. Добавить строгую валидацию email (проверка на недопустимые символы: `;`, `&`, `|`, `$`, `` ` ``, `\n`, и т.д.)
3. Никогда не передавать пользовательские данные напрямую в командную строку

## Пример исправления

```cpp
#include <spawn.h>
#include <sys/wait.h>

void send_password(const std::string &email, const std::string &password, const std::string &name) {
    // Валидация email
    if (!validate_email(email)) {
        log("Invalid email for password recovery");
        return;
    }

    char *argv[] = {
        const_cast<char*>("python3"),
        const_cast<char*>("change_pass.py"),
        const_cast<char*>(email.c_str()),
        const_cast<char*>(password.c_str()),
        const_cast<char*>(name.c_str()),
        nullptr
    };

    pid_t pid;
    posix_spawn(&pid, "/usr/bin/python3", nullptr, nullptr, argv, environ);
}
```
