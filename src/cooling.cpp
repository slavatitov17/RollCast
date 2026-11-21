#include "cooling.h"	// Подключает входные данные и результаты расчета
#include <algorithm>	// Предоставляет выбор масштаба энергии и температур
#include <cmath>	// Предоставляет логарифм и проверку конечности чисел
#include <limits>	// Предоставляет бесконечность для поиска температуры
#include <numbers>	// Предоставляет число пи для кольцевых ячеек
#include <stdexcept>	// Позволяет сообщить о недопустимых исходных данных
namespace {	// Оставляет детали схемы внутри расчетного модуля
constexpr double circle = 2 * std::numbers::pi;	// Задает множитель длины окружности
struct Cell {	// Описывает конечный объем на метр длины цилиндра
    double center;	// Задает радиус центра ячейки
    double outer;	// Задает радиус наружной грани ячейки
    double conductivity;	// Хранит теплопроводность материала ячейки
    double storage;	// Хранит теплоемкость объема на метр длины цилиндра
};	// Завершает объявление структуры или набора данных
bool positive(double value) {	// Проверяет конечное положительное значение
    return std::isfinite(value) && value > 0;	// Отклоняет нуль, бесконечность и отрицательные числа
}	// Завершает текущий блок обработки
double resistance(const Cell& cell, double radius) {	// Вычисляет сопротивление от центра ячейки до грани
    return std::abs(std::log(radius / cell.center)) / (circle * cell.conductivity);	// Использует радиальную формулу на метр длины
}	// Завершает текущий блок обработки
double storedEnergy(const std::vector<Cell>& grid, const std::vector<double>& degrees, double reference) {	// Суммирует тепловую энергию
    double energy = 0;	// Начинает суммирование с нуля
    for (std::size_t i = 0; i < grid.size(); ++i) {	// Обходит все кольцевые объемы
        energy += grid[i].storage * (degrees[i] - reference);	// Считает запас относительно температуры воздуха
    }	// Завершает текущий блок обработки
    return energy;	// Возвращает энергию на метр длины
}	// Завершает текущий блок обработки
void phaseStep(std::vector<double>& temperature, const std::vector<double>& conductance,	// Продвигает нелинейный тепловой расчет
    const std::vector<double>& storageRate, std::size_t metalCount, double latentCapacity,	// Принимает теплоемкости и отношение скрытой теплоты
    double air, PhaseChange phase, const std::vector<double>& exchange) {	// Принимает температуру среды и закон перехода
    const auto previous = temperature;	// Сохраняет температуры начала шага
    const std::size_t count = temperature.size();	// Определяет число уравнений температур
    std::vector<double> residual(count), diagonal(count), correction(count), candidate(count), upper(count);	// Хранит невязки и поправки температур
    const auto evaluate = [&](const std::vector<double>& values) {	// Вычисляет невязку баланса каждой ячейки
        double norm = 0;	// Начинает поиск максимальной относительной невязки
        for (std::size_t i = 0; i < count; ++i) {	// Выполняет прямой ход для поправок
            const double left = i == 0 ? values[i] : values[i - 1];	// Учитывает симметрию на оси
            const double right = i + 1 == count ? air : values[i + 1];	// Учитывает теплоотдачу к воздуху
            const double sensible = values[i] - previous[i];	// Находит изменение температуры за шаг
            const double latent = i < metalCount ? latentCapacity *	// Учитывает скрытую теплоту только в металле
                (phase.liquidShare(values[i]) - phase.liquidShare(previous[i])) : 0;	// Сравнивает объемные доли жидкости
            residual[i] = storageRate[i] * (sensible + latent)	// Записывает изменение полной энтальпии
                + conductance[i] * (values[i] - left) + conductance[i + 1] * (values[i] - right);	// Добавляет потоки через обе грани
            const double own = values[i] + latentCapacity * phase.liquidShare(values[i]);	// Вычисляет энтальпию деленную на теплоемкость
            const double leftEnthalpy = left + latentCapacity * phase.liquidShare(left);	// Читает энтальпию внутреннего соседа
            const double rightEnthalpy = right + latentCapacity * phase.liquidShare(right);	// Читает энтальпию внешнего соседа
            residual[i] += exchange[i] * (own - leftEnthalpy) + exchange[i + 1] * (own - rightEnthalpy);	// Добавляет консервативный обмен циркулирующего металла
            const double weight = storageRate[i] + conductance[i] + conductance[i + 1];	// Приводит невязку к температурному масштабу
            norm = std::max(norm, std::abs(residual[i]) / weight);	// Находит наибольшую нормированную невязку
        }	// Завершает текущий блок обработки
        return norm;	// Возвращает оценку точности текущей поправки
    };	// Завершает объявление структуры или набора данных
    constexpr double tolerance = 1e-9;	// Задает точность невязки в температурных единицах
    double error = evaluate(temperature);	// Вычисляет начальную невязку шага
    for (int repeat = 0; repeat < 80 && error > tolerance; ++repeat) {	// Уменьшает невязку методом Ньютона
        for (std::size_t i = 0; i < count; ++i) {	// Выполняет прямой ход для поправок
            const double slope = i < metalCount ? latentCapacity * phase.liquidSlope(temperature[i]) : 0;	// Находит фазовую часть производной
            diagonal[i] = storageRate[i] * (1 + slope) + conductance[i] + conductance[i + 1];	// Учитывает накопление и теплопроводность
            diagonal[i] += (exchange[i] + exchange[i + 1]) * (1 + slope);	// Учитывает исходящую энтальпию
            const double leftSlope = i > 0 ? latentCapacity * phase.liquidSlope(temperature[i - 1]) : 0;	// Находит производную энтальпии слева
            const double rightSlope = i + 1 < count ? latentCapacity * phase.liquidSlope(temperature[i + 1]) : 0;	// Находит производную энтальпии справа
            const double lower = -conductance[i] - exchange[i] * (1 + leftSlope);	// Формирует нижний коэффициент Якобиана
            upper[i] = -conductance[i + 1] - exchange[i + 1] * (1 + rightSlope);	// Формирует верхний коэффициент Якобиана
            correction[i] = -residual[i];	// Меняет знак невязки для поправки
            if (i > 0) {	// Исключает предыдущую неизвестную прогонкой
                const double ratio = lower / diagonal[i - 1];	// Находит множитель исключения
                diagonal[i] -= ratio * upper[i - 1];	// Обновляет диагональный коэффициент
                correction[i] -= ratio * correction[i - 1];	// Обновляет правую часть
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        correction.back() /= diagonal.back();	// Находит последнюю поправку температуры
        for (std::size_t i = count - 1; i > 0; --i) {	// Выполняет обратную прогонку
            correction[i - 1] = (correction[i - 1] - upper[i - 1] * correction[i]) / diagonal[i - 1];	// Восстанавливает предыдущую поправку
        }	// Завершает текущий блок обработки
        double multiplier = 1;	// Начинает с полной поправки Ньютона
        double candidateError = error;	// Хранит невязку пробной температуры
        for (int reduce = 0; reduce < 24; ++reduce) {	// Уменьшает поправку у границ фазового интервала
            for (std::size_t i = 0; i < count; ++i) {	// Выполняет прямой ход прогонки
                candidate[i] = temperature[i] + multiplier * correction[i];	// Не меняет принятое решение до проверки
            }	// Завершает текущий блок обработки
            candidateError = evaluate(candidate);	// Проверяет улучшение баланса энергии
            if (candidateError < error) {	// Принимает поправку при уменьшении невязки
                break;	// Завершает подбор величины поправки
            }	// Завершает текущий блок обработки
            multiplier /= 2;	// Делит пробную поправку пополам
        }	// Завершает текущий блок обработки
        if (!(candidateError < error)) {	// Проверяет возможность уменьшить невязку
            break;	// Останавливает расчет шага без сходимости
        }	// Завершает текущий блок обработки
        temperature.swap(candidate);	// Сохраняет улучшенное распределение температур
        error = candidateError;	// Обновляет нормированную невязку принятой поправки
    }	// Завершает текущий блок обработки
    if (!(error <= tolerance)) {	// Проверяет сходимость итераций при фазовом переходе
        throw std::runtime_error("Phase change did not converge");	// Сигнализирует о незавершенном шаге
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
CoolingResult solveCooling(std::span<const CoolingLayer> layers, CoolingBoundary boundary,	// Принимает слои и настройки теплоотдачи
    std::optional<PhaseChange> phase, std::optional<MeltCirculation> circulation, std::stop_token stop) {	// Решает одномерную задачу методом конечных объемов
    if (layers.empty() || layers.front().cells < 2 || !positive(boundary.duration) || !positive(boundary.maxStep)	// Проверяет сетку и время
        || !std::isfinite(boundary.airTemperature) || !std::isfinite(boundary.heatTransfer) || boundary.heatTransfer < 0 || !std::isfinite(boundary.metalContact) || boundary.metalContact < 0 || !std::isfinite(boundary.formContact) || boundary.formContact < 0) {	// Проверяет наружную границу
        throw std::invalid_argument("Invalid cooling boundary or metal grid");	// Отклоняет некорректную постановку
    }	// Завершает текущий блок обработки
    if (phase && (!std::isfinite(phase->solidus) || !std::isfinite(phase->liquidus)	// Проверяет конечность температурных границ
        || !(phase->liquidus > phase->solidus) || !positive(phase->latentHeat))) {	// Проверяет ширину интервала и скрытую теплоту
        throw std::invalid_argument("Invalid phase data");	// Отклоняет недопустимый фазовый интервал
    }	// Завершает текущий блок обработки
    if (circulation && (!phase || !positive(circulation->length) || !positive(circulation->viscosity) || !positive(circulation->expansion))) {	// Проверяет свойства модели циркуляции
        throw std::invalid_argument("Invalid circulation settings");	// Сообщает о недопустимых свойствах
    }	// Завершает текущий блок обработки
    std::vector<Cell> grid;	// Хранит ячейки от оси до воздуха
    std::vector<double> temperature;	// Хранит исходную температуру каждой ячейки
    std::vector<int> materialIndex;	// Сопоставляет ячейки материалам формы
    int layerIndex = 0;	// Начинает нумерацию с металлического слоя
    double radius = 0;	// Начинает построение сетки на оси
    for (const auto& layer : layers) {	// Последовательно добавляет металл и слои формы
        if (layer.cells < 1 || !positive(layer.thickness) || !positive(layer.conductivity)	// Проверяет геометрию и теплопроводность
            || !positive(layer.density) || !positive(layer.heatCapacity) || !std::isfinite(layer.initialTemperature)) {	// Проверяет тепловые свойства
            throw std::invalid_argument("Invalid cooling layer");	// Отклоняет некорректный слой
        }	// Завершает текущий блок обработки
        const double width = layer.thickness / layer.cells;	// Делит слой на равные интервалы по радиусу
        for (int i = 0; i < layer.cells; ++i) {	// Строит кольцевые ячейки слоя
            const double outer = radius + width;	// Находит наружную грань очередного кольца
            const double volume = std::numbers::pi * width * (outer + radius);	// Вычисляет объем кольца на метр длины
            grid.push_back({radius + width / 2, outer, layer.conductivity,	// Сохраняет геометрию и материал кольца
                            volume * layer.density * layer.heatCapacity});	// Вычисляет теплоемкость кольца
            materialIndex.push_back(layerIndex);	// Запоминает материал очередной ячейки
            temperature.push_back(layer.initialTemperature);	// Инициализирует температуру материала
            radius = outer;	// Переходит к следующей радиальной грани
        }	// Завершает текущий блок обработки
        ++layerIndex;	// Переходит к следующему материалу формы
    }	// Завершает текущий блок обработки
    const std::size_t count = grid.size();	// Учитывает все ячейки расчетной области
    std::vector<double> conductance(count + 1, 0);	// Хранит проводимости граней включая ось
    for (std::size_t i = 1; i < count; ++i) {	// Обходит внутренние грани сетки
        const double face = grid[i - 1].outer;	// Берет общий радиус между соседними ячейками
        double contact = 0;	// Начинает с идеального межъячеечного контакта
        if (materialIndex[i] != materialIndex[i - 1]) {	// Распознает границу двух материалов
            const double transfer = materialIndex[i] == 1 ? boundary.metalContact : boundary.formContact;	// Выбирает коэффициент теплообмена на контакте
            if (transfer > 0) {	// Включает заданное контактное сопротивление
                contact = 1 / (circle * face * transfer);	// Вычисляет сопротивление на метр длины
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        conductance[i] = 1 / (resistance(grid[i - 1], face) + contact + resistance(grid[i], face));	// Складывает сопротивления на общей грани
    }	// Завершает текущий блок обработки
    if (boundary.heatTransfer > 0) {	// Добавляет теплоотдачу при ненулевой конвекции
        const double convection = 1 / (circle * radius * boundary.heatTransfer);	// Вычисляет сопротивление наружного теплообмена
        conductance.back() = 1 / (resistance(grid.back(), radius) + convection);	// Объединяет теплопроводность и теплоотдачу
    }	// Завершает текущий блок обработки
    const double stepsRequired = std::ceil(boundary.duration / boundary.maxStep);	// Округляет число шагов вверх
    if (stepsRequired > 1000000) {	// Ограничивает объем сохраняемой истории температур
        throw std::invalid_argument("Too many cooling steps");	// Сообщает о слишком малом шаге времени
    }	// Завершает текущий блок обработки
    const auto steps = static_cast<std::size_t>(std::max(1.0, stepsRequired));	// Обеспечивает хотя бы один шаг
    const double dt = boundary.duration / static_cast<double>(steps);	// Равномерно делит заданную длительность расчета
    std::vector<double> diagonal(count), upper(count), storageRate(count);	// Хранит коэффициенты прогонки и тепловой инерции
    for (std::size_t i = 0; i < count; ++i) {	// Готовит общую матрицу для всех шагов
        storageRate[i] = grid[i].storage / dt;	// Делит теплоемкость ячейки на шаг
        diagonal[i] = storageRate[i] + conductance[i] + conductance[i + 1];	// Суммирует инерцию и проводимости ячейки
        if (i > 0) {	// Устраняет неизвестную предыдущей строки
            diagonal[i] += conductance[i] * upper[i - 1];	// Учитывает уже вычисленную строку матрицы
        }	// Завершает текущий блок обработки
        upper[i] = -conductance[i + 1] / diagonal[i];	// Сохраняет нормированный верхний коэффициент прогонки
    }	// Завершает текущий блок обработки
    CoolingResult result{};	// Создает историю рассчитанного охлаждения
    result.samples.reserve(steps + 1);	// Резервирует место для всех шагов
    const double start = layers.front().initialTemperature;	// Берет температуру металла после заполнения
    const double air = boundary.airTemperature;	// Сохраняет опорную температуру энергии
    const auto metalCount = static_cast<std::size_t>(layers.front().cells);	// Определяет число ячеек металла
    const auto metalEnd = metalCount - 1;	// Находит последнюю ячейку металла
    const double latentCapacity = phase ? phase->latentHeat / layers.front().heatCapacity : 0;	// Переводит скрытую теплоту в температурный масштаб
    const double metalResistance = resistance(grid[metalEnd], grid[metalEnd].outer);	// Находит сопротивление от центра до стенки металла
    const auto energy = [&](const std::vector<double>& values) {	// Вычисляет полную энергию на метр длины
        double total = storedEnergy(grid, values, air);	// Считает энергию нагрева всех слоев
        if (phase) {	// Выбирает энтальпийную схему при наличии фазового перехода
            for (std::size_t i = 0; i < metalCount; ++i) {	// Проходит температурные центры колец металла
                total += grid[i].storage * latentCapacity * phase->liquidShare(values[i]);	// Добавляет скрытую энергию жидкой части ячейки
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        return total;	// Возвращает полный запас энергии
    };	// Завершает объявление структуры или набора данных
    const double initialEnergy = energy(temperature);	// Определяет полный начальный запас энергии
    const double initialScale = std::max(1.0, std::abs(initialEnergy));	// Защищает нормировку при нулевой начальной энергии
    double escapedEnergy = 0;	// Накапливает тепло ушедшее наружу
    std::vector<RadialTemperature> profile(phase ? metalCount + 2 : 0);	// Резервирует точки металла оси и стенки
    double circulationSpeed = 0;	// Хранит скорость текущего шага
    std::vector<double> exchange(count + 1, 0);	// Хранит взаимные расходы энтальпии
    const auto record = [&](double seconds, double axis, double surface) {	// Сохраняет очередной расчетный кадр
        CoolingSample sample{seconds, axis, surface};	// Хранит время и температуры металла
        sample.circulationSpeed = circulationSpeed;	// Записывает масштаб скорости в историю
        if (phase) {	// Выбирает энтальпийную схему при наличии фазового перехода
            profile.front() = {0, axis};	// Устанавливает восстановленную температуру оси
            for (std::size_t i = 0; i < metalCount; ++i) {	// Проходит температурные центры колец металла
                profile[i + 1] = {grid[i].center, temperature[i]};	// Сохраняет радиус и температуру ячейки
            }	// Завершает текущий блок обработки
            profile.back() = {grid[metalEnd].outer, surface};	// Добавляет температуру боковой стенки
            sample.phase = summarizePhases(profile, *phase);	// Вычисляет зоны и интегральные фазовые доли
            if (boundary.recordProfile) {	// Сохраняет профиль только для визуализации
                sample.profile.reserve(profile.size());	// Выделяет компактную историю температур
                for (const auto& node : profile) { sample.profile.push_back(static_cast<float>(node.temperature)); }	// Копирует температуры узлов радиальной сетки
            }	// Завершает текущий блок обработки
            if (!result.completedAt && sample.phase.hottestTemperature <= phase->solidus) {	// Определяет достижение солидуса везде
                double completion = seconds;	// Принимает время текущего кадра
                if (!result.samples.empty()) {	// Уточняет время между соседними кадрами
                    const auto& prior = result.samples.back();	// Берет состояние перед полным затвердеванием
                    const double priorHot = prior.phase.hottestTemperature;	// Берет предыдущую наибольшую температуру
                    const double part = (priorHot - phase->solidus) / (priorHot - sample.phase.hottestTemperature);	// Интерполирует пересечение солидуса
                    completion = prior.seconds + (seconds - prior.seconds) * part;	// Вычисляет время полного затвердевания
                }	// Завершает текущий блок обработки
                result.completedAt = completion;	// Сохраняет первый момент полного затвердевания
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        result.samples.push_back(sample);	// Добавляет кадр в историю
    };	// Завершает объявление структуры или набора данных
    for (std::size_t i = 0; i < count; ++i) { if (grid[i].center >= boundary.ironRadius) { result.maxIronTemperature = std::max(result.maxIronTemperature, temperature[i]); } }	// Учитывает исходный нагрев металлической оснастки
    record(0, start, start);	// Записывает одинаковую начальную температуру металла
    std::vector<double> right(count);	// Хранит правую часть линейной схемы
    for (std::size_t step = 1; step <= steps; ++step) {	// Выполняет последовательные временные шаги
        if (stop.stop_requested()) {	// Проверяет отмену устаревшего фонового расчета
            throw std::runtime_error("Расчет отменен");	// Завершает работу без публикации неполной истории
        }	// Завершает текущий блок обработки
        if (phase) {	// Выбирает энтальпийную схему при наличии фазового перехода
            std::fill(exchange.begin(), exchange.end(), 0);	// Начинает с закрытых циркуляционных связей
            circulationSpeed = 0;	// Сбрасывает скорость для неподвижного режима
            if (circulation) {	// Включает учебную модель движения
                const double metalRadius = grid[metalEnd].outer;	// Получает радиус отделения
                double hottest = phase->solidus;	// Начинает оценку теплой жидкой ветви
                double coldest = std::numeric_limits<double>::infinity();	// Начинает оценку холодной жидкой ветви
                for (std::size_t i = 0; i < metalCount; ++i) {	// Обходит металл с остатками жидкости
                    if (phase->liquidShare(temperature[i]) > 0) {	// Исключает полностью затвердевшие ячейки
                        hottest = std::max(hottest, temperature[i]);	// Запоминает максимальную температуру жидкости
                        coldest = std::min(coldest, temperature[i]);	// Запоминает минимальную температуру жидкости
                    }	// Завершает текущий блок обработки
                }	// Завершает текущий блок обработки
                const double diameter = metalRadius / 2;	// Принимает учебный гидравлический диаметр
                const double loop = 2 * (circulation->length + metalRadius);	// Задает длину замкнутого контура
                const double drive = 9.81 * circulation->expansion * std::max(0.0, hottest - coldest) * circulation->length / loop;	// Оценивает ускорение от плавучести
                const double drag = 32 * circulation->viscosity / (layers.front().density * diameter * diameter);	// Оценивает линейное вязкое торможение
                const double loss = 2 / loop;	// Принимает учебные местные потери контура
                circulationSpeed = drive == 0 ? 0 : 2 * drive / (drag + std::sqrt(drag * drag + 4 * loss * drive));	// Решает стационарный баланс сил
                const double mixingLength = metalRadius / 4;	// Принимает учебную длину перемешивания
                for (std::size_t i = 1; i < metalCount; ++i) {	// Соединяет только соседние металлические ячейки
                    const double liquid = std::min(phase->liquidShare(temperature[i - 1]), phase->liquidShare(temperature[i]));	// Оценивает доступность жидкого пути
                    const double diffusivity = circulationSpeed * mixingLength * liquid * liquid * liquid / 3;	// Замыкает перенос энтальпии перемешиванием
                    exchange[i] = circle * grid[i - 1].outer * layers.front().density * layers.front().heatCapacity * diffusivity / (grid[i].center - grid[i - 1].center);	// Переводит перемешивание в межъячеечный обмен
                }	// Завершает текущий блок обработки
            }	// Завершает текущий блок обработки
            phaseStep(temperature, conductance, storageRate, metalCount, latentCapacity, air, *phase, exchange);	// Решает нелинейный баланс энтальпии
        } else {	// Выбирает линейную теплопроводность с постоянной теплоемкостью
            for (std::size_t i = 0; i < count; ++i) {	// Выполняет прямой ход прогонки
                double value = storageRate[i] * temperature[i];	// Учитывает температуру на предыдущем шаге
                if (i > 0) {	// Добавляет вклад предыдущего уравнения
                    value += conductance[i] * right[i - 1];	// Устраняет известный коэффициент слева
                }	// Завершает текущий блок обработки
                if (i + 1 == count) {	// Выбирает последнюю ячейку у воздуха
                    value += conductance.back() * air;	// Включает температуру окружающей среды
                }	// Завершает текущий блок обработки
                right[i] = value / diagonal[i];	// Нормирует правую часть текущей строки
            }	// Завершает текущий блок обработки
            temperature.back() = right.back();	// Начинает обратный ход с последнего уравнения
            for (std::size_t i = count - 1; i > 0; --i) {	// Проходит от стенки формы к оси
                temperature[i - 1] = right[i - 1] - upper[i - 1] * temperature[i];	// Подставляет найденную температуру соседней ячейки
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
        const double neighbor = (metalEnd + 1 < count) ? temperature[metalEnd + 1] : air;	// Находит температуру за металлом
        const double flow = conductance[metalEnd + 1] * (temperature[metalEnd] - neighbor);	// Вычисляет поток через стенку металла
        const double surface = temperature[metalEnd] - flow * metalResistance;	// Восстанавливает температуру на самой грани металла
        const double axis = (9 * temperature[0] - temperature[1]) / 8;	// Экстраполирует к оси с нулевым градиентом
        for (std::size_t i = 0; i < count; ++i) { if (grid[i].center >= boundary.ironRadius) { result.maxIronTemperature = std::max(result.maxIronTemperature, temperature[i]); } }	// Проверяет нагрев оснастки на каждом шаге
        record(step * dt, axis, surface);	// Записывает температуры и фазовое состояние
        escapedEnergy += dt * conductance.back() * (temperature.back() - air);	// Интегрирует наружный тепловой поток
        if (boundary.stopWhenSolid && result.completedAt) { break; }	// Завершает поиск на первом твердом кадре
    }	// Завершает текущий блок обработки
    const double finalEnergy = energy(temperature);	// Вычисляет остаток теплоты в модели
    result.relativeEnergyError = std::abs(initialEnergy - finalEnergy - escapedEnergy) / initialScale;	// Проверяет замыкание теплового баланса
    return result;	// Возвращает температуры и показатель сохранения энергии
}	// Завершает текущий блок обработки
CoolingResult demonstrationCooling(int diameterMm, std::optional<PhaseChange> phase, std::optional<MeltCirculation> circulation) {	// Формирует учебный набор слоев для бочки
    const CoolingLayer layers[]{	// Размещает металл, песок и опоку от оси
        {diameterMm / 2000.0, 32, 7800, 650, 1550, 64},	// Переводит диаметр металла в радиус в метрах
        {0.10, 0.8, 1600, 1000, 20, 16},	// Задает условные свойства и толщину песчаного слоя
        {0.025, 45, 7800, 500, 20, 8},	// Задает условные свойства и толщину металлической опоки
    };	// Завершает объявление структуры или набора данных
    const double duration = (phase ? 36 : 12) * 3600.0;	// Увеличивает интервал наблюдения за фазовым переходом
    return solveCooling(layers, {20, 12, duration, 30}, phase, circulation);	// Запускает учебный расчет с выбранной тепловой схемой
}	// Завершает текущий блок обработки
