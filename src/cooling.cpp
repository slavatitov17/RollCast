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
void phaseStep(std::vector<double>& temperature, const std::vector<double>& conductance,									// Продвигает нелинейный тепловой расчёт
    const std::vector<double>& storageRate, std::size_t metalCount, double latentCapacity,									// Принимает теплоёмкости и отношение скрытой теплоты
    double air, PhaseChange phase) {																						// Принимает температуру среды и закон перехода
    const auto previous = temperature;																						// Сохраняет температуры начала шага
    const std::size_t count = temperature.size();																			// Определяет число уравнений температур
    std::vector<double> residual(count), diagonal(count), correction(count), candidate(count);								// Хранит невязки и поправки температур
    const auto evaluate = [&](const std::vector<double>& values) {															// Вычисляет невязку баланса каждой ячейки
        double norm = 0;																									// Начинает поиск максимальной относительной невязки
        for (std::size_t i = 0; i < count; ++i) {																			// Обходит ячейки тепловой модели
            const double left = i == 0 ? values[i] : values[i - 1];															// Учитывает симметрию на оси
            const double right = i + 1 == count ? air : values[i + 1];														// Учитывает теплоотдачу к воздуху
            const double sensible = values[i] - previous[i];																// Находит изменение температуры за шаг
            const double latent = i < metalCount ? latentCapacity *															// Учитывает скрытую теплоту только в металле
                (phase.liquidShare(values[i]) - phase.liquidShare(previous[i])) : 0;										// Сравнивает объёмные доли жидкости
            residual[i] = storageRate[i] * (sensible + latent)																// Записывает изменение полной энтальпии
                + conductance[i] * (values[i] - left) + conductance[i + 1] * (values[i] - right);							// Добавляет потоки через обе грани
            const double weight = storageRate[i] + conductance[i] + conductance[i + 1];										// Приводит невязку к температурному масштабу
            norm = std::max(norm, std::abs(residual[i]) / weight);															// Находит наибольшую нормированную невязку
        }																													// Завершает обработку всех ячеек
        return norm;																										// Возвращает оценку точности текущей поправки
    };																														// Завершает описание функции невязки
    constexpr double tolerance = 1e-9;																						// Задаёт точность невязки в температурных единицах
    double error = evaluate(temperature);																					// Вычисляет начальную невязку шага
    for (int repeat = 0; repeat < 80 && error > tolerance; ++repeat) {														// Уменьшает невязку методом Ньютона
        for (std::size_t i = 0; i < count; ++i) {																			// Выполняет прямой ход для поправок
            const double slope = i < metalCount ? latentCapacity * phase.liquidSlope(temperature[i]) : 0;					// Находит фазовую часть производной
            diagonal[i] = storageRate[i] * (1 + slope) + conductance[i] + conductance[i + 1];								// Вычисляет главную диагональ Якобиана
            correction[i] = -residual[i];																					// Направляет поправку против текущей невязки
            if (i > 0) {																									// Устраняет нижний соседний коэффициент
                const double ratio = conductance[i] / diagonal[i - 1];														// Находит множитель предыдущего уравнения
                diagonal[i] -= ratio * conductance[i];																		// Корректирует диагональный коэффициент прогонки
                correction[i] += ratio * correction[i - 1];																	// Корректирует правую часть прогонки
            }																												// Завершает исключение неизвестной
        }																													// Завершает прямой ход метода прогонки
        correction.back() /= diagonal.back();																				// Вычисляет наружную поправку температуры
        for (std::size_t i = count - 1; i > 0; --i) {																		// Считает поправки обратно к оси
            correction[i - 1] = (correction[i - 1] + conductance[i] * correction[i]) / diagonal[i - 1];						// Подставляет известную соседнюю поправку
        }																													// Завершает обратный ход метода прогонки
        double multiplier = 1;																								// Начинает с полной поправки Ньютона
        double candidateError = error;																						// Хранит невязку пробной температуры
        for (int reduce = 0; reduce < 24; ++reduce) {																		// Уменьшает поправку у границ фазового интервала
            for (std::size_t i = 0; i < count; ++i) {																		// Применяет пробную поправку ко всем ячейкам
                candidate[i] = temperature[i] + multiplier * correction[i];													// Не меняет принятое решение до проверки
            }																												// Завершает построение пробного решения
            candidateError = evaluate(candidate);																			// Проверяет улучшение баланса энергии
            if (candidateError < error) {																					// Принимает поправку при уменьшении невязки
                break;																										// Завершает подбор величины поправки
            }																												// Завершает проверку улучшения
            multiplier /= 2;																								// Делит пробную поправку пополам
        }																													// Завершает поиск принимаемой поправки
        if (!(candidateError < error)) {																					// Проверяет возможность уменьшить невязку
            break;																											// Останавливает расчёт шага без сходимости
        }																													// Завершает проверку допустимой поправки
        temperature.swap(candidate);																						// Сохраняет улучшенное распределение температур
        error = candidateError;																								// Обновляет нормированную невязку принятой поправки
    }																														// Завершает нелинейные итерации шага
    if (!(error <= tolerance)) {																							// Проверяет сходимость итераций при фазовом переходе
        throw std::runtime_error("Phase change did not converge");															// Сигнализирует о незавершённом шаге
    }																														// Завершает проверку сходимости
}																															// Завершает решение энтальпийного шага
}																															// Завершает область локальных функций
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary,									// Принимает слои и настройки теплоотдачи
    std::optional<PhaseChange> phase) {																						// Решает одномерную задачу методом конечных объёмов
    if (layers.empty() || layers.front().cells < 2 || !positive(boundary.duration) || !positive(boundary.maxStep)			// Проверяет сетку и время
        || !std::isfinite(boundary.airTemperature) || !std::isfinite(boundary.heatTransfer) || boundary.heatTransfer < 0) {	// Проверяет наружную границу
        throw std::invalid_argument("Invalid cooling boundary or metal grid");												// Отклоняет некорректную постановку
    }																														// Завершает проверку общих параметров
    if (phase && (!std::isfinite(phase->solidus) || !std::isfinite(phase->liquidus)											// Проверяет конечность температурных границ
        || !(phase->liquidus > phase->solidus) || !positive(phase->latentHeat))) {											// Проверяет ширину интервала и скрытую теплоту
        throw std::invalid_argument("Invalid phase data");																	// Отклоняет недопустимый фазовый интервал
    }																														// Завершает проверку параметров фазового перехода
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
    const double air = boundary.airTemperature;																				// Сохраняет опорную температуру энергии
    const auto metalCount = static_cast<std::size_t>(layers.front().cells);													// Определяет число ячеек металла
    const auto metalEnd = metalCount - 1;																					// Находит последнюю ячейку металла
    const double latentCapacity = phase ? phase->latentHeat / layers.front().heatCapacity : 0;								// Переводит скрытую теплоту в температурный масштаб
    const double metalResistance = resistance(grid[metalEnd], grid[metalEnd].outer);										// Находит сопротивление от центра до стенки металла
    const auto energy = [&](const std::vector<double>& values) {															// Вычисляет полную энергию на метр длины
        double total = storedEnergy(grid, values, air);																		// Считает энергию нагрева всех слоёв
        if (phase) {																										// Добавляет скрытую теплоту жидкого металла
            for (std::size_t i = 0; i < metalCount; ++i) {																	// Обходит только кольца бочки
                total += grid[i].storage * latentCapacity * phase->liquidShare(values[i]);									// Добавляет скрытую энергию жидкой части ячейки
            }																												// Завершает суммирование скрытой теплоты
        }																													// Завершает учёт фазовой составляющей
        return total;																										// Возвращает полный запас энергии
    };																														// Завершает определение полной внутренней энергии
    const double initialEnergy = energy(temperature);																		// Определяет полный начальный запас энергии
    const double initialScale = std::max(1.0, std::abs(initialEnergy));														// Защищает нормировку при нулевой начальной энергии
    double escapedEnergy = 0;																								// Накапливает тепло ушедшее наружу
    std::vector<RadialTemperature> profile(phase ? metalCount + 2 : 0);														// Резервирует точки металла оси и стенки
    const auto record = [&](double seconds, double axis, double surface) {													// Сохраняет очередной расчётный кадр
        CoolingSample sample{seconds, axis, surface};																		// Хранит время и температуры металла
        if (phase) {																										// Проверяет необходимость расчёта фаз
            profile.front() = {0, axis};																					// Устанавливает восстановленную температуру оси
            for (std::size_t i = 0; i < metalCount; ++i) {																	// Проходит температурные центры колец металла
                profile[i + 1] = {grid[i].center, temperature[i]};															// Сохраняет радиус и температуру ячейки
            }																												// Завершает восстановление центра профиля
            profile.back() = {grid[metalEnd].outer, surface};																// Добавляет температуру боковой стенки
            sample.phase = summarizePhases(profile, *phase);																// Вычисляет толщины и объёмы фазовых зон
            if (!result.completedAt && sample.phase.hottestTemperature <= phase->solidus) {									// Определяет достижение солидуса везде
                double completion = seconds;																				// Принимает время текущего кадра
                if (!result.samples.empty()) {																				// Уточняет время между соседними кадрами
                    const auto& prior = result.samples.back();																// Берёт состояние перед полным затвердеванием
                    const double priorHot = prior.phase.hottestTemperature;													// Берёт предыдущую наибольшую температуру
                    const double part = (priorHot - phase->solidus) / (priorHot - sample.phase.hottestTemperature);			// Интерполирует пересечение солидуса
                    completion = prior.seconds + (seconds - prior.seconds) * part;											// Вычисляет время полного затвердевания
                }																											// Завершает уточнение момента полного затвердевания
                result.completedAt = completion;																			// Сохраняет первый момент полного затвердевания
            }																												// Завершает определение момента затвердевания всей бочки
        }																													// Завершает расчёт фазовых показателей кадра
        result.samples.push_back(sample);																					// Добавляет кадр в историю
    };																														// Завершает подготовку кадров расчёта
    record(0, start, start);																								// Записывает одинаковую начальную температуру металла
    std::vector<double> right(count);																						// Хранит правую часть линейной схемы
    for (std::size_t step = 1; step <= steps; ++step) {																		// Продвигает решение во времени
        if (phase) {																										// Выбирает энтальпийную схему при наличии фазового перехода
            phaseStep(temperature, conductance, storageRate, metalCount, latentCapacity, air, *phase);						// Решает нелинейный баланс энтальпии
        } else {																											// Выбирает линейную теплопроводность с постоянной теплоёмкостью
            for (std::size_t i = 0; i < count; ++i) {																		// Выполняет прямой ход прогонки
                double value = storageRate[i] * temperature[i];																// Учитывает температуру на предыдущем шаге
                if (i > 0) {																								// Добавляет вклад предыдущего уравнения
                    value += conductance[i] * right[i - 1];																	// Устраняет известный коэффициент слева
                }																											// Завершает обработку предыдущей строки
                if (i + 1 == count) {																						// Выбирает последнюю ячейку у воздуха
                    value += conductance.back() * air;																		// Включает температуру окружающей среды
                }																											// Завершает обработку наружной границы
                right[i] = value / diagonal[i];																				// Нормирует правую часть текущей строки
            }																												// Завершает прямой ход прогонки
            temperature.back() = right.back();																				// Начинает обратный ход с последнего уравнения
            for (std::size_t i = count - 1; i > 0; --i) {																	// Проходит от стенки формы к оси
                temperature[i - 1] = right[i - 1] - upper[i - 1] * temperature[i];											// Подставляет найденную температуру соседней ячейки
            }																												// Завершает восстановление температур всех ячеек
        }																													// Завершает выбор схемы изменения температуры
        const double neighbor = (metalEnd + 1 < count) ? temperature[metalEnd + 1] : air;									// Находит температуру за металлом
        const double flow = conductance[metalEnd + 1] * (temperature[metalEnd] - neighbor);									// Вычисляет поток через стенку металла
        const double surface = temperature[metalEnd] - flow * metalResistance;												// Восстанавливает температуру на самой грани металла
        const double axis = (9 * temperature[0] - temperature[1]) / 8;														// Экстраполирует к оси с нулевым градиентом
        record(step * dt, axis, surface);																					// Записывает температуры и фазовое состояние
        escapedEnergy += dt * conductance.back() * (temperature.back() - air);												// Интегрирует наружный тепловой поток
    }																														// Завершает временной цикл
    const double finalEnergy = energy(temperature);																			// Вычисляет остаток теплоты в модели
    result.relativeEnergyError = std::abs(initialEnergy - finalEnergy - escapedEnergy) / initialScale;						// Проверяет замыкание теплового баланса
    return result;																											// Возвращает температуры и показатель сохранения энергии
}																															// Завершает численный расчёт охлаждения
CoolingResult demonstrationCooling(int diameterMm, std::optional<PhaseChange> phase) {										// Формирует учебный набор слоёв для бочки
    const CoolingLayer layers[]{																							// Размещает металл, песок и опоку от оси
        {diameterMm / 2000.0, 32, 7800, 650, 1550, 64},																		// Переводит диаметр металла в радиус в метрах
        {0.10, 0.8, 1600, 1000, 20, 16},																					// Задаёт условные свойства и толщину песчаного слоя
        {0.025, 45, 7800, 500, 20, 8},																						// Задаёт условные свойства и толщину металлической опоки
    };																														// Завершает учебный набор материалов
    const double duration = (phase ? 36 : 12) * 3600.0;																		// Увеличивает интервал наблюдения за фазовым переходом
    return solveCooling(layers, {20, 12, duration, 30}, phase);																// Запускает учебный расчёт с выбранной тепловой схемой
}																															// Завершает расчёт учебного примера
