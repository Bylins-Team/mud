# ISSUE-017: Слабая инициализация генератора случайных чисел (PRNG)

**Тип**: Weak Random Number Generation
**Критичность**: LOW
**Файл**: `src/administration/name_adviser.cpp:8`
**Статус**: Активна

## Описание

В конструкторе класса `NameAdviser` используется старая C-функция `srand()` с инициализацией через `time(nullptr)`, что является слабой практикой для генерации случайных чисел:

```cpp
NameAdviser::NameAdviser() {
	std::srand(static_cast<unsigned int>((std::time(nullptr))));
}
```

Проблемы:
1. **Предсказуемость**: `time(nullptr)` возвращает время в секундах - легко предсказуемое значение
2. **Низкая энтропия**: При запуске программы в одну и ту же секунду seed будет одинаковым
3. **Устаревший API**: `srand()` и `rand()` - устаревшие C-функции, не рекомендуемые для C++
4. **Глобальное состояние**: `srand()` изменяет глобальное состояние, что может привести к проблемам при многопоточности

## Воздействие

В контексте `NameAdviser` это не является критичной уязвимостью безопасности, так как класс используется только для предложения имен новым игрокам. Однако:

1. Злоумышленник может предсказать, какие имена будут предложены
2. При быстром перезапуске сервера может повторяться одна и та же последовательность имен
3. В методе `get_random_name_list()` на строке 83 используется правильный современный генератор `std::mt19937(std::random_device()())`, но в конструкторе остался устаревший код

## Воспроизведение

```bash
# Запустить сервер
./circle

# В течение той же секунды перезапустить
killall circle
./circle

# Последовательность "случайных" имен будет одинаковой
```

## Решение

**Вариант 1**: Удалить `srand()` полностью, так как она не используется
```cpp
NameAdviser::NameAdviser() {
	// Инициализация не требуется - используем std::random_device в get_random_name_list()
}
```

**Вариант 2**: Если все же нужна инициализация, использовать C++ random
```cpp
class NameAdviser {
private:
	std::mt19937 m_rng;

public:
	NameAdviser() : m_rng(std::random_device{}()) {
		// Инициализация генератора с хорошим seed
	}

	std::vector<std::string> get_random_name_list() {
		// ...
		std::shuffle(unique_numbers.begin(), unique_numbers.end(), m_rng);
		// ...
	}
};
```

**Вариант 3**: Использовать более высокоэнтропийный seed
```cpp
NameAdviser::NameAdviser() {
	std::random_device rd;
	std::srand(rd());
	// Но все равно лучше использовать вариант 1 или 2
}
```

## Анализ существующего кода

В методе `get_random_name_list()` (строка 83) уже используется правильный подход:
```cpp
std::shuffle(unique_numbers.begin(), unique_numbers.end(), std::mt19937(std::random_device()()));
```

Это делает вызов `srand()` в конструкторе бесполезным и вводящим в заблуждение.

## Рекомендации

1. **Немедленно**: Удалить строку 8 (`std::srand(...)`) как не используемую
2. Рассмотреть создание member-переменной `std::mt19937` для избежания повторной инициализации `std::random_device()` при каждом вызове `get_random_name_list()`
3. Проверить остальной код на использование устаревших `rand()` и `srand()`

## Дополнительная оптимизация

Текущий код создает новый `std::mt19937` и `std::random_device` при каждом вызове `get_random_name_list()`:
```cpp
std::shuffle(unique_numbers.begin(), unique_numbers.end(), std::mt19937(std::random_device()()));
```

Это неэффективно. Лучше создать их один раз в конструкторе:
```cpp
class NameAdviser {
private:
	std::random_device m_rd;
	std::mt19937 m_gen;

public:
	NameAdviser() : m_rd(), m_gen(m_rd()) {}

	std::vector<std::string> get_random_name_list() {
		// ...
		std::shuffle(unique_numbers.begin(), unique_numbers.end(), m_gen);
		// ...
	}
};
```
