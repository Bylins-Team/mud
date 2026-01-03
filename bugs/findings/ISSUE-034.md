# ISSUE-034: Возможность зависания сервера из-за пропущенных пульсов

**Файл:** `/home/kvirund/repos/mud/src/engine/core/comm.cpp`
**Функция:** `game_loop()`
**Строки:** 1356-1382
**Серьезность:** СРЕДНЯЯ
**Категория:** Denial of Service / Performance
**CVE потенциал:** Да (DoS)

## Описание проблемы

В главном игровом цикле `game_loop()` есть механизм обработки пропущенных пульсов (missed pulses), который может привести к зависанию сервера при длительной задержке.

### Проблемный код:

```cpp
void game_loop(int epoll, socket_t mother_desc) {
	// ...
	while (!shutting_down()) {
		// ... расчет пропущенных пульсов ...

		if (missed_pulses <= 0) {
			log("SYSERR: **BAD** MISSED_PULSES NONPOSITIVE (%d), TIME GOING BACKWARDS!!", missed_pulses);
			missed_pulses = 1;
		}

		// If we missed more than 30 seconds worth of pulses, just do 30 secs
		// �������� �� 4 ���
		// �������� �� 1 ��� -- ������� �� ������ ������ :)
		if (missed_pulses > (1 * kPassesPerSec)) {  // <-- 1 СЕКУНДА!
			const auto missed_seconds = missed_pulses / kPassesPerSec;
			const auto current_pulse = GlobalObjects::heartbeat().pulse_number();
			char tmpbuf[256];
			sprintf(tmpbuf,"WARNING: Missed %d seconds worth of pulses (%d) on the pulse %d.",
				static_cast<int>(missed_seconds), missed_pulses, current_pulse);
			mudlog(tmpbuf, LGH, kLvlImmortal, SYSLOG, true);
			missed_pulses = 1 * kPassesPerSec;  // <-- Ограничение 1 секунда
		}

		// Now execute the heartbeat functions
		while (missed_pulses--) {  // <-- Может выполняться до 25 раз (kPassesPerSec = 25)
#ifdef HAS_EPOLL
			process_io(epoll, mother_desc, events);
#else
			process_io(input_set, output_set, exc_set, null_set, mother_desc, maxdesc);
#endif
			GlobalObjects::heartbeat()(missed_pulses);
		}
	}
}
```

## Анализ проблемы

### 1. Магические числа в коде

```cpp
// Комментарии говорят о разных значениях:
// "If we missed more than 30 seconds" - комментарий
// "�������� �� 4 ���"                 - комментарий на русском
// "�������� �� 1 ���"                 - комментарий на русском
// if (missed_pulses > (1 * kPassesPerSec))  - фактический код (1 секунда!)

// kPassesPerSec предположительно = 25 (из описания в CLAUDE.md: 25Hz = 40ms tick)
```

**Проблема:** комментарии и код не совпадают, что указывает на многократные изменения без обновления документации.

### 2. Ограничение в 1 секунду слишком мало

Если сервер "заморожен" на 2+ секунды (например, из-за свапинга или долгой операции):
```
1. Сервер просыпается после 5-секундной задержки
2. missed_pulses = 5 * 25 = 125
3. Код ограничивает до 1 * 25 = 25
4. Обрабатывается только 25 пульсов
5. Теряется 100 пульсов игрового времени
6. Игровые таймеры, аффекты, события рассинхронизируются
```

### 3. Потенциал для DoS

Атакующий может специально создать нагрузку:
```
1. Вызвать длительную операцию (например, сложный DG Script)
2. Сервер зависнет на несколько секунд
3. Пульсы будут потеряны
4. Повторить многократно
5. Накопленная потеря пульсов приведет к:
   - Рассинхронизации игровых механик
   - Потере событий (респаун мобов, декей предметов)
   - Неконсистентному состоянию мира
```

### 4. Проблема со sprintf в критическом цикле

```cpp
char tmpbuf[256];
sprintf(tmpbuf,"WARNING: Missed %d seconds...", ...);
mudlog(tmpbuf, LGH, kLvlImmortal, SYSLOG, true);
```

**Проблемы:**
- `sprintf` небезопасен (хотя здесь вероятность переполнения низкая)
- `mudlog()` может быть медленным (запись в файл, отправка игрокам)
- Выполняется в главном цикле при КАЖДОМ превышении лимита
- Может усугубить проблему производительности

## Сценарии эксплуатации

### Сценарий 1: DoS через DG Scripts

```
1. Создать DG Script с бесконечным циклом или очень долгим выполнением
2. Триггер вызывается на каждом пульсе
3. Сервер зависает на обработке скрипта
4. Пульсы пропускаются
5. Игровой мир рассинхронизируется
```

### Сценарий 2: DoS через множество подключений

```
1. Открыть 1000+ соединений
2. Отправлять данные одновременно со всех
3. process_io() зависнет на обработке
4. Пульсы пропускаются
5. Достаточно повторить несколько раз
```

### Сценарий 3: Эксплуатация потери пульсов

```
1. Вызвать пропуск пульсов
2. Таймеры заклинаний сбиваются
3. Cooldown скиллов не отрабатывает корректно
4. Игрок может использовать скиллы чаще чем должен
```

## Дополнительные проблемы

### 1. Отсутствие метрик

Нет:
- Подсчета общего количества потерянных пульсов
- Статистики задержек
- Профилирования что именно вызвало задержку

### 2. Неинформативное логирование

```cpp
sprintf(tmpbuf,"WARNING: Missed %d seconds worth of pulses (%d) on the pulse %d.",
	static_cast<int>(missed_seconds), missed_pulses, current_pulse);
```

Не логируется:
- Причина задержки
- Стек вызовов
- Какая операция заняла много времени

### 3. Небезопасный sprintf

```cpp
char tmpbuf[256];
sprintf(tmpbuf, ...);  // <-- Должно быть snprintf
```

Хотя переполнение маловероятно, это плохая практика.

## Рекомендации по исправлению

### Исправление 1: Увеличить лимит и исправить sprintf

```cpp
// Константы в начале файла
const int kMaxMissedPulsesSeconds = 5;  // 5 секунд максимум

// В game_loop():
if (missed_pulses <= 0) {
	log("SYSERR: **BAD** MISSED_PULSES NONPOSITIVE (%d), TIME GOING BACKWARDS!!", missed_pulses);
	missed_pulses = 1;
}

// Ограничить пропущенные пульсы разумным значением
if (missed_pulses > (kMaxMissedPulsesSeconds * kPassesPerSec)) {
	const auto missed_seconds = missed_pulses / kPassesPerSec;
	const auto current_pulse = GlobalObjects::heartbeat().pulse_number();

	// Безопасное логирование
	log("WARNING: Missed %d seconds worth of pulses (%d) on pulse %d. "
	    "Limiting to %d seconds (%d pulses).",
	    static_cast<int>(missed_seconds), missed_pulses, current_pulse,
	    kMaxMissedPulsesSeconds, kMaxMissedPulsesSeconds * kPassesPerSec);

	missed_pulses = kMaxMissedPulsesSeconds * kPassesPerSec;
}
```

### Исправление 2: Добавить метрики и профилирование

```cpp
// Глобальные счетчики
static unsigned long total_missed_pulses = 0;
static unsigned long max_missed_pulses = 0;
static time_t last_lag_spike = 0;

// В game_loop():
if (missed_pulses > 1) {
	total_missed_pulses += (missed_pulses - 1);
	if (missed_pulses > max_missed_pulses) {
		max_missed_pulses = missed_pulses;
		last_lag_spike = time(nullptr);
	}
}

// Добавить команду для админов показывающую статистику
// do_lagstat() показывающую total_missed_pulses, max_missed_pulses, etc.
```

### Исправление 3: Разбить обработку на части

```cpp
// Вместо обработки всех пропущенных пульсов за раз,
// обрабатывать по частям с проверками между ними

const int kMaxPulsesPerIteration = 50;  // Не более 50 пульсов за раз

while (missed_pulses > 0) {
	int pulses_to_process = std::min(missed_pulses, kMaxPulsesPerIteration);

	for (int i = 0; i < pulses_to_process; i++) {
		process_io(epoll, mother_desc, events);
		GlobalObjects::heartbeat()(missed_pulses - i - 1);
	}

	missed_pulses -= pulses_to_process;

	// Проверить не нужно ли прервать обработку
	if (shutting_down()) {
		break;
	}

	// Дать возможность обработать новые соединения
	// между порциями пропущенных пульсов
}
```

### Исправление 4: Добавить watchdog

```cpp
// Детектировать долгие операции и предупреждать

#include <chrono>

auto pulse_start = std::chrono::steady_clock::now();

// Обработка пульса
GlobalObjects::heartbeat()(missed_pulses);

auto pulse_end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(pulse_end - pulse_start);

if (duration.count() > 100) {  // Пульс должен быть < 100ms
	log("WARNING: Pulse %d took %ld ms to process!",
	    GlobalObjects::heartbeat().pulse_number(),
	    duration.count());
}
```

## Связанные проблемы

- Проблемы производительности в heartbeat системе
- Долгие операции в DG Scripts
- Неоптимальный process_io()

## Статус

- [x] Обнаружена
- [ ] Исправлена
- [ ] Протестирована

## Приоритет

**СРЕДНИЙ - рекомендуется исправить**

Может привести к рассинхронизации игрового мира и DoS, но требует специфических условий для эксплуатации.
