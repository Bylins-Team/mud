# ISSUE-002: Передача паролей открытым текстом по email

**Тип**: Information Disclosure
**Критичность**: HIGH
**Файл**: `src/administration/password.cpp:65-87`
**Статус**: Активна

## Описание

Система отправляет пользовательские пароли в открытом виде по email через внешний скрипт. Это грубое нарушение безопасности:
- Пароли передаются в незашифрованном виде
- Пароли могут быть перехвачены на пути к email серверу
- Пароли сохраняются в почтовых ящиках пользователей

## Воспроизведение

1. Пользователь запрашивает восстановление пароля
2. Система генерирует пароль и отправляет его открытым текстом на email
3. Злоумышленник с доступом к email серверу или почтовому ящику получает пароль

## Решение

1. **Правильный подход**: Отправлять одноразовую ссылку для сброса пароля с токеном
2. Токен должен:
   - Быть случайным и криптографически стойким
   - Иметь ограниченное время жизни (15-30 минут)
   - Быть одноразовым (удаляться после использования)
3. Никогда не отправлять пароли по email в любом виде

## Пример исправления

```cpp
std::string generate_reset_token() {
    // Генерация криптографически стойкого токена
    unsigned char token_bytes[32];
    RAND_bytes(token_bytes, sizeof(token_bytes));

    std::stringstream ss;
    for (auto byte : token_bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

void send_password_reset_link(const std::string &email, const std::string &name) {
    std::string token = generate_reset_token();
    time_t expiry = time(nullptr) + 1800; // 30 минут

    // Сохранить token и expiry в БД для пользователя
    save_reset_token(email, token, expiry);

    // Отправить ссылку вида: https://mud.ru/reset?token=<token>
    std::string link = "https://mud.ru/reset?token=" + token;
    send_email(email, "Password Reset", "Click here to reset: " + link);
}
```
