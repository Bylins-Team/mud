# Отправка паролей в открытом виде по email

**Файл**: src/administration/password.cpp
**Строки**: 65-87
**Критичность**: HIGH

## Проблема
Функции `send_password()`, `set_password_to_email()` и `set_all_password_to_email()` отправляют пароли пользователей в открытом виде по электронной почте:

```cpp
void send_password(std::string email, std::string password, std::string name) {
    std::string cmd_line = "python3 change_pass.py " + email + " " + password + " " + name + " &";
    auto result = system(cmd_line.c_str());
}

void set_password_to_email(CharData *ch, const std::string &pwd) {
    ch->set_passwd(generate_md5_hash(pwd));
    send_password(std::string(GET_EMAIL(ch)), pwd.c_str(), std::string(GET_NAME(ch)));
    //                                         ^^^^^^^^^ - открытый пароль!
}

void set_all_password_to_email(const char *email, const std::string &pwd, const std::string &name) {
    send_password(std::string(email), pwd.c_str(), name.c_str());
    //                                ^^^^^^^^^ - открытый пароль!
}
```

## Проблемы безопасности
1. **Email не зашифрован**: Письма могут быть перехвачены на любом этапе доставки (SMTP сервера, ISP, публичный Wi-Fi)
2. **Email хранится бесконечно**: Пароль остаётся в почтовом ящике пользователя и на почтовых серверах
3. **Компрометация почты**: Если злоумышленник получит доступ к почте, он сразу получит пароль
4. **Логи email-серверов**: Пароли могут попасть в логи SMTP серверов

## Стандарты безопасности
Отправка паролей по email нарушает:
- OWASP Top 10 (A02:2021 – Cryptographic Failures)
- PCI DSS требования
- GDPR требования к защите персональных данных
- Общепринятые security best practices

## Решение
**Вместо отправки пароля использовать систему сброса пароля:**

```cpp
// Генерация одноразового токена
std::string generate_password_reset_token(CharData *ch) {
    // Генерируем криптографически стойкий токен
    std::string token = generate_secure_random_token(32);

    // Сохраняем токен с TTL (например, 1 час)
    save_reset_token(ch, token, time(nullptr) + 3600);

    return token;
}

// Отправка ссылки для сброса пароля
void send_password_reset_link(CharData *ch) {
    std::string token = generate_password_reset_token(ch);
    std::string reset_link = "https://mud.ru/reset-password?token=" + token;

    // Отправляем только ссылку, НЕ пароль
    send_email(GET_EMAIL(ch),
               "Password Reset Request",
               "Click here to reset your password: " + reset_link);
}

// Проверка токена и установка нового пароля
void reset_password_with_token(const std::string &token, const std::string &new_password) {
    CharData *ch = find_character_by_token(token);
    if (!ch || is_token_expired(token)) {
        return;  // Токен недействителен или истёк
    }

    set_password(ch, new_password);
    invalidate_token(token);
}
```

**Альтернатива для систем без web-интерфейса:**
Отправлять временный код (PIN), который работает только один раз и истекает через короткое время (5-15 минут).

**ВАЖНО**: Никогда не отправляйте настоящие пароли по email!
