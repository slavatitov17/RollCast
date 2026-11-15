#include "cooling.h"																										// Подключает входные данные и результаты расчёта
#include <algorithm>																										// Предоставляет выбор масштаба энергии и температур
#include <cmath>																											// Предоставляет логарифм и проверку конечности чисел
#include <numbers>																											// Предоставляет число пи для кольцевых ячеек
#include <stdexcept>																										// Позволяет сообщить о недопустимых исходных данных
namespace {																													// Оставляет детали схемы внутри расчётного модуля
constexpr double circle = 2 * std::numbers::pi;																				// Задаёт множитель длины окружности
struct Cell {																												// Описывает конечный объём на метр длины цилиндра
    double center;																											// Задаёт радиус центра ячейки
    double outer;																											// Задаёт радиус наружной грани ячейки
    double conductivity;																									// Хранит теплопроводность материала ячейки
    double storage;																											// Хранит теплоёмкость объёма на метр длины цилиндра
};																															// Завершает описание ячейки
bool positive(double value) {																								// Проверяет конечное положительное значение
    return std::isfinite(value) && value > 0;																				// Отклоняет нуль, бесконечность и отрицательные числа
}																															// Завершает проверку физического параметра
double resistance(const Cell& cell, double radius) {																		// Вычисляет сопротивление от центра ячейки до грани
    return std::abs(std::log(radius / cell.center)) / (circle * cell.conductivity);											// Использует радиальную формулу на метр длины
}																															// Завершает расчёт сопротивления
double storedEnergy(const std::vector<Cell>& grid, const std::vector<double>& degrees, double reference) {					// Суммирует тепловую энергию
    double energy = 0;																										// Начинает суммирование с нуля
    for (std::size_t i = 0; i < grid.size(); ++i) {																			// Обходит все кольцевые объёмы
        energy += grid[i].storage * (degrees[i] - reference);																// Считает запас относительно температуры воздуха
    }																														// Завершает объёмное суммирование
    return energy;																											// Возвращает энергию на метр длины
}																															// Завершает расчёт запаса энергии
}																															// Завершает область локальных функций
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary) {								// Решает одномерную задачу методом конечных объёмов
    if (layers.empty() || layers.front().cells < 2 || !positive(boundary.duration) || !positive(boundary.maxStep)			// Проверяет сетку и время
        || !std::isfinite(boundary.airTemperature) || !std::isfinite(boundary.heatTransfer) || boundary.heatTransfer < 0) {	// Проверяет наружную границу
        throw std::invalid_argument("Invalid cooling boundary or metal grid");												// Отклоняет некорректную постановку
    }																														// Завершает проверку общих параметров
    std::vector<Cell> grid;																									// Хранит ячейки от оси до воздуха
    std::vector<double> temperature;																						// Хранит исходную температуру каждой ячейки
    double radius = 0;																										// Начинает построение сетки на оси
    for (const auto& layer : layers) {																						// Последовательно добавляет металл и слои формы
        if (layer.cells < 1 || !positive(layer.thickness) || !positive(layer.conductivity)									// Проверяет геометрию и теплопроводность
            || !positive(layer.density) || !positive(layer.heatCapacity) || !std::isfinite(layer.initialTemperature)) {		// Проверяет тепловые свойства
            throw std::invalid_argument("Invalid cooling layer");															// Отклоняет некорректный слой
        }																													// Завершает проверку слоя
        const double width = layer.thickness / layer.cells;																	// Делит слой на равные интервалы по радиусу
        for (int i = 0; i < layer.cells; ++i) {																				// Строит кольцевые ячейки слоя
            const double outer = radius + width;																			// Находит наружную грань очередного кольца
            const double volume = std::numbers::pi * width * (outer + radius);												// Вычисляет объём кольца на метр длины
            grid.push_back({radius + width / 2, outer, layer.conductivity,													// Сохраняет геометрию и материал кольца
                            volume * layer.density * layer.heatCapacity});													// Вычисляет теплоёмкость кольца
            temperature.push_back(layer.initialTemperature);																// Инициализирует температуру материала
            radius = outer;																									// Переходит к следующей радиальной грани
        }																													// Завершает разбиение слоя
    }																														// Завершает построение сетки слоёв
    const std::size_t count = grid.size();																					// Учитывает все ячейки расчётной области
    std::vector<double> conductance(count + 1, 0);																			// Хранит проводимости граней включая ось
    for (std::size_t i = 1; i < count; ++i) {																				// Обходит внутренние грани сетки
        const double face = grid[i - 1].outer;																				// Берёт общий радиус между соседними ячейками
        conductance[i] = 1 / (resistance(grid[i - 1], face) + resistance(grid[i], face));									// Складывает два полукольцевых сопротивления
    }																														// Завершает расчёт межъячеечных проводимостей
    if (boundary.heatTransfer > 0) {																						// Добавляет теплоотдачу при ненулевой конвекции
        const double convection = 1 / (circle * radius * boundary.heatTransfer);											// Вычисляет сопротивление наружного теплообмена
        conductance.back() = 1 / (resistance(grid.back(), radius) + convection);											// Объединяет теплопроводность и теплоотдачу
    }																														// Завершает расчёт проводимости на наружной границе
    const double stepsRequired = std::ceil(boundary.duration / boundary.maxStep);											// Округляет число шагов вверх
    if (stepsRequired > 1000000) {																							// Ограничивает объём сохраняемой истории температур
        throw std::invalid_argument("Too many cooling steps");																// Сообщает о слишком малом шаге времени
    }																														// Завершает проверку размера истории
    const auto steps = static_cast<std::size_t>(std::max(1.0, stepsRequired));												// Обеспечивает хотя бы один шаг
    const double dt = boundary.duration / static_cast<double>(steps);														// Равномерно делит заданную длительность расчёта
    std::vector<double> diagonal(count), upper(count), storageRate(count);													// Хранит коэффициенты прогонки и тепловой инерции
    for (std::size_t i = 0; i < count; ++i) {																				// Готовит общую матрицу для всех шагов
        storageRate[i] = grid[i].storage / dt;																				// Делит теплоёмкость ячейки на шаг
        diagonal[i] = storageRate[i] + conductance[i] + conductance[i + 1];													// Суммирует инерцию и проводимости ячейки
        if (i > 0) {																										// Устраняет неизвестную предыдущей строки
            diagonal[i] += conductance[i] * upper[i - 1];																	// Учитывает уже вычисленную строку матрицы
        }																													// Завершает исключение нижней диагонали
        upper[i] = -conductance[i + 1] / diagonal[i];																		// Сохраняет нормированный верхний коэффициент прогонки
    }																														// Завершает подготовку трёхдиагональной матрицы
    CoolingResult result{};																									// Создаёт историю рассчитанного охлаждения
    result.samples.reserve(steps + 1);																						// Резервирует место для всех шагов
    const double start = layers.front().initialTemperature;																	// Берёт температуру металла после заполнения
    result.samples.push_back({0, start, start});																			// Записывает одинаковую начальную температуру металла
    const double air = boundary.airTemperature;																				// Сохраняет опорную температуру энергии
    const double initialEnergy = storedEnergy(grid, temperature, air);														// Вычисляет начальный запас теплоты
    double escapedEnergy = 0;																								// Накапливает тепло, ушедшее наружу
    const auto metalEnd = static_cast<std::size_t>(layers.front().cells - 1);												// Находит последнюю ячейку металла
    const double metalResistance = resistance(grid[metalEnd], grid[metalEnd].outer);										// Находит сопротивление от центра до стенки металла
    const double initialScale = std::max(1.0, std::abs(initialEnergy));														// Защищает нормировку при нулевой начальной энергии
    std::vector<double> right(count);																						// Хранит правую часть и результат прямого хода
    for (std::size_t step = 1; step <= steps; ++step) {																		// Продвигает решение во времени
        for (std::size_t i = 0; i < count; ++i) {																			// Выполняет прямой ход прогонки
            double value = storageRate[i] * temperature[i];																	// Учитывает температуру на предыдущем шаге
            if (i > 0) {																									// Добавляет вклад предыдущего уравнения
                value += conductance[i] * right[i - 1];																		// Устраняет известный коэффициент слева
            }																												// Завершает обработку предыдущей строки
            if (i + 1 == count) {																							// Выбирает последнюю ячейку у воздуха
                value += conductance.back() * air;																			// Включает температуру окружающей среды
            }																												// Завершает обработку наружной границы
            right[i] = value / diagonal[i];																					// Нормирует правую часть текущей строки
        }																													// Завершает прямой ход прогонки
        temperature.back() = right.back();																					// Начинает обратный ход с последнего уравнения
        for (std::size_t i = count - 1; i > 0; --i) {																		// Проходит от стенки формы к оси
            temperature[i - 1] = right[i - 1] - upper[i - 1] * temperature[i];												// Подставляет найденную температуру соседней ячейки
        }																													// Завершает восстановление температур всех ячеек
        const double neighbor = (metalEnd + 1 < count) ? temperature[metalEnd + 1] : air;									// Находит температуру за металлом
        const double flow = conductance[metalEnd + 1] * (temperature[metalEnd] - neighbor);									// Вычисляет поток через стенку металла
        const double surface = temperature[metalEnd] - flow * metalResistance;												// Восстанавливает температуру на самой грани металла
        const double axis = (9 * temperature[0] - temperature[1]) / 8;														// Экстраполирует к оси с нулевым градиентом
        result.samples.push_back({step * dt, axis, surface});																// Записывает осевую и поверхностную температуры
        escapedEnergy += dt * conductance.back() * (temperature.back() - air);												// Интегрирует наружный тепловой поток
    }																														// Завершает временной цикл
    const double finalEnergy = storedEnergy(grid, temperature, air);														// Вычисляет остаток теплоты в модели
    result.relativeEnergyError = std::abs(initialEnergy - finalEnergy - escapedEnergy) / initialScale;						// Проверяет замыкание теплового баланса
    return result;																											// Возвращает температуры и показатель сохранения энергии
}																															// Завершает численный расчёт охлаждения
CoolingResult demonstrationCooling(int diameterMm) {																		// Формирует учебный набор слоёв для бочки
    const CoolingLayer layers[]{																							// Размещает металл, песок и опоку от оси
        {diameterMm / 2000.0, 32, 7800, 650, 1550, 64},																		// Переводит диаметр металла в радиус в метрах
        {0.10, 0.8, 1600, 1000, 20, 16},																					// Задаёт условные свойства и толщину песчаного слоя
        {0.025, 45, 7800, 500, 20, 8},																						// Задаёт условные свойства и толщину металлической опоки
    };																														// Завершает учебный набор материалов
    return solveCooling(layers, {20, 12, 12 * 3600, 30});																	// Рассчитывает двенадцать часов с шагом тридцать секунд
}																															// Завершает расчёт учебного примера
