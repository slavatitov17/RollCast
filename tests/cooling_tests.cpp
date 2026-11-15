#include "cooling.h"																									// Подключает расчёт охлаждения для контрольных примеров
#include <cmath>																										// Предоставляет функции Бесселя и экспоненту
#include <iostream>																										// Выводит результаты численной проверки
#include <stdexcept>																									// Сообщает о нарушении условий теста
namespace {																												// Делает вспомогательные функции локальными для теста
void require(bool condition, const char* message) {																		// Проверяет условие независимо от конфигурации сборки
    if (!condition) {																									// Обнаруживает неудачную проверку
        throw std::runtime_error(message);																				// Передаёт причину отказа в точку запуска
    }																													// Завершает проверку условия
}																														// Завершает вспомогательную проверку
void checkAnalyticalCylinder() {																						// Сравнивает нестационарное охлаждение с аналитическим решением
    constexpr double radius = 0.46;																						// Задаёт радиус контрольного цилиндра
    constexpr double conductivity = 32;																					// Задаёт теплопроводность контрольного материала
    constexpr double density = 7800;																					// Задаёт плотность контрольного материала
    constexpr double capacity = 650;																					// Задаёт удельную теплоёмкость материала
    constexpr double ambient = 20;																						// Задаёт температуру окружающего воздуха
    constexpr double initial = 1500;																					// Задаёт равномерную начальную температуру цилиндра
    const double j0 = std::cyl_bessel_j(0, 1.0);																		// Вычисляет функцию Бесселя нулевого порядка
    const double j1 = std::cyl_bessel_j(1, 1.0);																		// Вычисляет функцию Бесселя первого порядка
    const double h = conductivity * j1 / (radius * j0);																	// Подбирает число Био с первым корнем единица
    const double duration = radius * radius * density * capacity / conductivity;										// Достигает числа Фурье равного единице
    const double amplitude = 2 * j1 / (j0 * j0 + j1 * j1);																// Задаёт первый коэффициент аналитического ряда
    const double axis = ambient + (initial - ambient) * amplitude * std::exp(-1.0);										// Вычисляет аналитическую температуру на оси
    const double surface = ambient + (axis - ambient) * j0;																// Вычисляет аналитическую температуру на границе
    CoolingLayer cylinder{radius, conductivity, density, capacity, initial, 32};										// Задаёт исходную грубую сетку
    const auto coarse = solveCooling({&cylinder, 1}, {ambient, h, duration, 120});										// Вычисляет решение на грубой сетке
    cylinder.cells = 96;																								// Увеличивает число колец для проверки сходимости
    const auto fine = solveCooling({&cylinder, 1}, {ambient, h, duration, 15});											// Уменьшает радиальный и временной шаги
    const auto& last = fine.samples.back();																				// Берёт конечный отсчёт точной сетки
    const double errorAxis = std::abs(last.axisTemperature - axis);														// Измеряет ошибку осевой температуры
    const double errorSurface = std::abs(last.surfaceTemperature - surface);											// Измеряет ошибку поверхностной температуры
    require(errorAxis < 0.4 && errorSurface < 0.4, "Analytical cylinder mismatch");										// Проверяет обе температуры по аналитическому ряду
    require(errorAxis < std::abs(coarse.samples.back().axisTemperature - axis), "No axial convergence");				// Проверяет эффект измельчения сетки
    require(fine.relativeEnergyError < 1e-8, "Convective energy imbalance");											// Проверяет сохранение тепловой энергии
    std::cout << "Analytical errors, C: " << errorAxis << ' ' << errorSurface << '\n';									// Печатает ошибки в двух точках
}																														// Завершает проверку по аналитическому решению
void checkComposite() {																									// Проверяет охлаждение через два слоя формы
    const auto result = demonstrationCooling(920);																		// Вычисляет учебный пример бочки
    const auto& first = result.samples.front();																			// Берёт начальный отсчёт температур
    const auto& last = result.samples.back();																			// Берёт конечный отсчёт через двенадцать часов
    require(first.axisTemperature == 1550 && first.surfaceTemperature == 1550, "Invalid initial state");				// Проверяет исходную температуру металла
    require(result.samples.size() == 1441 && last.seconds == 43200, "Invalid time history");							// Проверяет шаг и длительность расчёта
    require(last.surfaceTemperature > 20 && last.surfaceTemperature < last.axisTemperature, "Invalid radial order");	// Проверяет направление охлаждения в бочке
    require(last.axisTemperature < 1550 && result.relativeEnergyError < 1e-8, "Invalid cooling balance");				// Проверяет потери теплоты и баланс
    require(demonstrationCooling(700).samples.back().axisTemperature < last.axisTemperature, "Diameter had no effect");	// Проверяет влияние диаметра бочки
    std::cout << "Demo final axis and wall, C: " << last.axisTemperature << ' ' << last.surfaceTemperature << '\n';		// Печатает результаты учебного примера
}																														// Завершает проверку учебной формы
void checkInsulation() {																								// Проверяет обмен теплоты в изолированной системе
    const CoolingLayer layers[]{{0.2, 35, 7800, 650, 1500, 64}, {0.1, 1, 1500, 900, 20, 24}};							// Задаёт два слоя разной температуры
    const auto insulated = solveCooling(layers, {20, 0, 6000, 30});														// Отключает наружный поток теплоты
    require(insulated.relativeEnergyError < 1e-8, "Insulated energy imbalance");										// Проверяет сохранение общей внутренней энергии
    require(insulated.samples.back().surfaceTemperature < 1500, "No interlayer heat transfer");							// Проверяет перенос энергии между слоями
    const CoolingLayer uniform{0.2, 35, 7800, 650, 20, 64};																// Задаёт цилиндр при температуре воздуха
    const auto equilibrium = solveCooling({&uniform, 1}, {20, 12, 6000, 30});											// Проверяет исходное тепловое равновесие
    require(std::abs(equilibrium.samples.back().axisTemperature - 20) < 1e-8, "Stationary state changed");				// Проверяет сохранение температуры равновесия
}																														// Завершает проверку граничных условий
}																														// Завершает область контрольных примеров
int main() {																											// Запускает контрольные примеры расчётного модуля
    try {																												// Перехватывает ошибки контрольных проверок
        checkAnalyticalCylinder();																						// Проверяет точность и сходимость численного цилиндра
        checkComposite();																								// Проверяет теплообмен металла с формой
        checkInsulation();																								// Проверяет равновесие и тепловой баланс
    } catch (const std::exception& error) {																				// Получает причину ошибки проверки
        std::cerr << error.what() << '\n';																				// Выводит причину отказа в CTest
        return 1;																										// Сообщает о неудачной проверке
    }																													// Завершает обработку контрольных примеров
    return 0;																											// Подтверждает успех всех проверок
}																														// Завершает тест расчёта охлаждения
