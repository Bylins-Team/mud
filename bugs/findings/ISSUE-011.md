# ISSUE-011: Использование raw pointer вместо shared_ptr

**Тип**: Memory Management
**Критичность**: MEDIUM
**Файл**: `src/administration/names.cpp:449-458`
**Статус**: Активна

## Описание

В функции `do_name()` создаётся объект `Player` через `new` вместо использования `shared_ptr`, что противоречит руководству проекта (CLAUDE.md указывает использовать CharData::shared_ptr):

```cpp
vict = new Player; // TODO: перенести на стек
if (LoadPlayerCharacter(name.c_str(), vict, ELoadCharFlags::kFindId) < 0) {
    SendMsgToChar("Такого персонажа не существует.\r\n", ch);
    delete vict;
    return;  // Правильное удаление
}
go_name(ch, vict, action);
vict->save_char();
delete vict;  // Правильное удаление
```

Хотя память освобождается корректно, этот паттерн опасен:
- Если добавится новый `return` или исключение между `new` и `delete`, будет утечка
- Не соответствует принципам RAII
- Противоречит стилю кодирования проекта

## Воспроизведение

1. Администратор выполняет команду `имя <игрок> одобрить`
2. Игрок не найден или функция `LoadPlayerCharacter` вызывает исключение
3. Если между `new` и `delete` возникнет исключение - утечка памяти
4. При будущих модификациях кода легко забыть добавить `delete` на новых путях выхода

## Решение

**Вариант 1** (используя shared_ptr):
```cpp
auto vict = std::make_shared<Player>();
if (LoadPlayerCharacter(name.c_str(), vict.get(), ELoadCharFlags::kFindId) < 0) {
    SendMsgToChar("Такого персонажа не существует.\r\n", ch);
    return;  // Автоматическое удаление
}
go_name(ch, vict.get(), action);
vict->save_char();
// Автоматическое удаление при выходе из scope
```

**Вариант 2** (как указано в TODO - на стеке):
```cpp
Player vict_obj;
Player* vict = &vict_obj;
if (LoadPlayerCharacter(name.c_str(), vict, ELoadCharFlags::kFindId) < 0) {
    SendMsgToChar("Такого персонажа не существует.\r\n", ch);
    return;
}
go_name(ch, vict, action);
vict->save_char();
// Автоматическое удаление при выходе из scope
```
