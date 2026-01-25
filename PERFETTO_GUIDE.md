# Perfetto Integration Guide

## Обзор

Проект интегрирован с [Perfetto](https://perfetto.dev/) - мощной системой трассировки и профилирования от Google, используемой в Android и Chrome.

## Возможности

- **Трассировка событий**: Записывайте временные метки начала/конца операций
- **Метрики**: Счетчики, гистограммы, gauge'ы
- **Категории**: Группируйте события по типам (game, network, db, combat)
- **Визуализация**: Просмотр traces в веб-интерфейсе Perfetto UI

## Сборка с Perfetto

### Включение Perfetto

```bash
cd build
cmake -DWITH_PERFETTO=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Отключение Perfetto

```bash
cmake -DWITH_PERFETTO=OFF ..
make -j$(nproc)
```

Без `WITH_PERFETTO` все вызовы API становятся no-op (нулевые накладные расходы).

## Конфигурация

В `lib/misc/configuration.xml`:

```xml
<perfetto>
    <enabled>true</enabled>
    <trace_file>/tmp/bylins-trace.perfetto-trace</trace_file>
</perfetto>
```

Или через переменную окружения:

```bash
export PERFETTO_TRACE_FILE=/path/to/trace.perfetto-trace
./circle
```

## Использование API

### Трассировка операций

```cpp
#include "engine/observability/perfetto_wrapper.h"

void process_command(CharData *ch, const char *cmd) {
    // Автоматическая трассировка с RAII
    auto trace = observability::PerfettoWrapper::instance().begin_trace("process_command");
    trace->set_attribute("command", cmd);
    trace->set_attribute("player_id", ch->get_uid());
    
    // ... выполнение команды ...
    
    // Trace автоматически закрывается при выходе из scope
}
```

### Метрики

```cpp
auto &perfetto = observability::PerfettoWrapper::instance();

// Счетчик (монотонно растущий)
perfetto.increment_counter("commands_total", 1);

// Gauge (текущее значение)
perfetto.set_gauge("players_online", player_count);

// Гистограмма (распределение значений)
perfetto.record_histogram("command_duration_ms", duration);
```

### С метками (labels)

```cpp
std::map<std::string, std::string> labels{
    {"command_type", "combat"},
    {"zone", "newbie"}
};

perfetto.increment_counter("combat_actions", 1, labels);
```

## Просмотр результатов

### 1. Запустите игру

Trace файл будет создан автоматически при shutdown.

### 2. Откройте Perfetto UI

В браузере: https://ui.perfetto.dev/

### 3. Загрузите trace файл

- Нажмите **"Open trace file"**
- Выберите `/tmp/bylins-trace.perfetto-trace`

### 4. Анализируйте

- **Timeline view**: Временная шкала всех событий
- **Zooming**: Колесо мыши для масштабирования
- **Selection**: Кликайте на события для деталей
- **Search**: `Ctrl+F` для поиска по именам событий
- **Metrics**: Вкладка "Metrics" для агрегированных данных

## Примеры интеграции

### Трассировка сетевого ввода/вывода

```cpp
int process_input(DescriptorData *d) {
    auto trace = observability::PerfettoWrapper::instance()
        .begin_trace("network.process_input");
    trace->set_attribute("descriptor", d->descriptor);
    
    // ... обработка ...
    
    return bytes_read;
}
```

### Трассировка боевой системы

```cpp
void perform_violence() {
    auto trace = observability::PerfettoWrapper::instance()
        .begin_trace("combat.perform_violence");
    
    for (auto ch : combat_list) {
        auto fight_trace = observability::PerfettoWrapper::instance()
            .begin_trace("combat.fight_round");
        fight_trace->set_attribute("char_id", ch->get_uid());
        
        // ... логика боя ...
    }
}
```

### Трассировка загрузки мира

```cpp
void boot_world() {
    auto trace = observability::PerfettoWrapper::instance()
        .begin_trace("db.boot_world");
    
    {
        auto zone_trace = perfetto.begin_trace("db.load_zones");
        // ... загрузка зон ...
    }
    
    {
        auto mob_trace = perfetto.begin_trace("db.load_mobs");
        // ... загрузка мобов ...
    }
}
```

## Категории событий

Perfetto поддерживает категории для группировки событий:

- `game` - игровая логика (по умолчанию)
- `network` - сетевой ввод/вывод
- `db` - операции с базой данных
- `combat` - боевая система

Категории определены в `perfetto_wrapper.cpp`:

```cpp
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("game").SetDescription("Game logic events"),
    perfetto::Category("network").SetDescription("Network I/O events"),
    perfetto::Category("db").SetDescription("Database operations"),
    perfetto::Category("combat").SetDescription("Combat system events")
);
```

## Производительность

- **С выключенным Perfetto** (`WITH_PERFETTO=OFF`): Нулевые накладные расходы
- **С включенным Perfetto**:
  - Инициализация: ~1ms
  - Trace event: ~100-500ns
  - Counter/Gauge: ~50-200ns
  - Буфер в памяти: 10MB (настраивается)
  - Запись файла: при shutdown (~100ms)

## Troubleshooting

### Perfetto не компилируется

Проверьте требования:
- CMake >= 3.15
- C++17 или выше (у вас C++20 ✅)
- Доступ к интернету для FetchContent

### Файл trace не создается

1. Проверьте права записи в `/tmp/`
2. Убедитесь, что `enabled=true` в конфигурации
3. Проверьте лог при startup: "Perfetto initialized successfully"

### Trace файл пустой

- Perfetto записывает данные только при `shutdown()`
- Убедитесь, что игра завершается корректно (не SIGKILL)

## Дополнительная информация

- Официальная документация: https://perfetto.dev/docs/
- Perfetto UI: https://ui.perfetto.dev/
- GitHub: https://github.com/google/perfetto

## Пример trace файла

После запуска игры с Perfetto вы увидите:

```
[startup] Perfetto initialized successfully: trace_file=/tmp/bylins-trace.perfetto-trace
[gameplay] ...normal game logs...
[shutdown] Shutting down Perfetto...
[shutdown] Perfetto trace saved to /tmp/bylins-trace.perfetto-trace (1.2 MB)
```

Откройте этот файл в https://ui.perfetto.dev/ для анализа!
