#ifndef IDENTIFY_EXPORT_IDENTIFY_SIMULATION_H
#define IDENTIFY_EXPORT_IDENTIFY_SIMULATION_H

#include "exporter.h"
#include <string>

// Симуляция команды опознать (identify) для объекта
// fullness - уровень навыка опознания (0, 20, 30, 40, 60, 75, 90, 100, 400)
// Возвращает текстовое описание предмета в формате игры
std::string SimulateIdentify(const SimpleObject& obj, int fullness);

#endif // IDENTIFY_EXPORT_IDENTIFY_SIMULATION_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
