#!/bin/bash
# Скрипт для быстрого определения с какого файла продолжить

echo "=== СТАТУС АНАЛИЗА ==="
echo ""

# Текущий прогресс
TOTAL=896
DONE=$(grep -c "^\[" bugs/PROGRESS.txt 2>/dev/null || echo 0)
FOUND=$(ls bugs/findings/*.md 2>/dev/null | wc -l)

echo "Всего файлов: $TOTAL"
echo "Проанализировано: $DONE"
echo "Проблем найдено: $FOUND"
echo ""

# Последний файл
LAST_LINE=$(grep "^\[" bugs/PROGRESS.txt | tail -1)
echo "Последний: $LAST_LINE"
echo ""

# Следующий файл
LAST_NUM=$(echo "$LAST_LINE" | grep -oP '^\[\K[0-9]+')
NEXT_NUM=$((LAST_NUM + 1))

if [ $NEXT_NUM -le $TOTAL ]; then
    NEXT_FILE=$(sed -n "${NEXT_NUM}p" bugs/all_files.txt)
    echo "Следующий файл: [$NEXT_NUM/896] $NEXT_FILE"
else
    echo "✅ Анализ завершён!"
fi
