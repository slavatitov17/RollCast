#pragma once																			// Защищает заголовок от повторного подключения
#ifndef NOMINMAX																		// Проверяет настройку имен стандартных функций
#define NOMINMAX																		// Отключает макросы Windows для максимума и минимума
#endif																					// Завершает выбор настройки заголовка Windows
#include <windows.h>																	// Объявляет контекст рисования и прямоугольник Windows
#include <vector>																		// Хранит области подсказок в порядке рисования
#include "project.h"																																		// Подключает общие размеры пяти отделений
enum class MoldPart {																	// Перечисляет зоны разреза с подсказками
    none, profit,																				// Обозначает свободное пространство вокруг формы
    riser, upperModel, funnel, pipe, upperFrame, rod,									// Перечисляет верхние элементы формы и сифона
    plug, chill, barrel, lowerModelMiddle, middleFrame,									// Перечисляет элементы вокруг бочки
    lowerModelBottom, flask, plate, gate, elbow											// Перечисляет нижние элементы литниковой системы
};																						// Завершает перечень элементов формы
class MoldHitMap {																		// Хранит точные экранные области нарисованных элементов
public:																					// Открывает операции рисования и выбора части формы
    MoldHitMap() = default;																// Создаёт пустую карту областей
    MoldHitMap(const MoldHitMap&) = delete;												// Запрещает копирование дескрипторов областей
    MoldHitMap& operator=(const MoldHitMap&) = delete;									// Запрещает совместное владение областями
    ~MoldHitMap();																		// Освобождает области при закрытии окна
    void clear();																		// Удаляет области предыдущего кадра
    void add(HRGN region, MoldPart part);												// Принимает область нарисованного элемента
    MoldPart partAt(POINT cursor) const;												// Возвращает верхний элемент под указателем
    void addFluid(HRGN region, bool casting);																												// Сохраняет полости отдельно от оснастки
    HRGN casting = nullptr;																																	// Хранит объединённую полость отливки
    HRGN runners = nullptr;																																	// Хранит объединённые внутренние литниковые каналы
private:																				// Скрывает детали хранения областей Windows
    struct Area {																		// Связывает экранную область с частью формы
        HRGN region;																	// Хранит дескриптор области Windows
        MoldPart part;																	// Хранит имя выбранной зоны
    };																					// Завершает описание экранной области
    std::vector<Area> areas;															// Хранит области в порядке рисования
};																						// Завершает описание карты подсказок
struct MoldFrame {																																			// Задаёт окраску и заполнение формы
    double filling = 1;																																		// Хранит прогресс заливки от нуля до единицы
    std::array<double, sectionCount> filled{1, 1, 1, 1, 1};																												// Хранит заполнение цилиндрических отделений
    std::array<const CoolingSample*, sectionCount> samples{};																											// Передаёт рассчитанные профили общего момента
    PhaseChange phase = demoPhase;																															// Хранит фазовые границы текущего проекта
};																																							// Завершает состояние визуализации металла
void drawMold(HDC device, const RECT& area, const Project& project,																				// Принимает область и геометрию разреза
    MoldHitMap& hits, MoldPart hovered, POINT cursor, const MoldFrame& frame = {}, bool rebuild = true);													// Рисует форму и текущее состояние металла
void drawMoldLabel(HDC device, const RECT& area, MoldPart part, POINT cursor);																				// Рисует подпись поверх кэшированной формы
