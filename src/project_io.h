#pragma once	// Защищает интерфейс файловых операций
#include "mold_cooling.h"	// Подключает проект и рассчитанные истории
#include <filesystem>	// Поддерживает пути с русскими именами
struct Run {	// Объединяет входные данные с рассчитанным результатом
    Project project;	// Хранит исходные данные именно этого результата
    MoldCooling cooling;	// Хранит четыре согласованные температурные истории
};	// Завершает объявление структуры или набора данных
Project loadProject(const std::filesystem::path& path);	// Читает и проверяет проект из файла
void saveProject(const std::filesystem::path& path, const Project& project, const Run* run = nullptr);	// Сохраняет входы и доступную итоговую сводку
void exportWorkbook(const std::filesystem::path& path, const Run& run);	// Сохраняет параметры объемы толщины и графики Excel
