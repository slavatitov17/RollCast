import bisect	# Находит соседние расчетные кадры для независимой интерполяции
import json	# Читает исходные числа расчетного ядра
import math	# Проверяет округление и конечность экспортных значений
import pathlib	# Работает только с выделенным каталогом проверки
import re	# Разбирает ссылки серий на ячейки Excel
import subprocess	# Запускает проверочный генератор действующего экспортера
import sys	# Получает пути из команды CTest
import zipfile	# Независимо проверяет двоичный контейнер XLSX
import xml.etree.ElementTree as ET	# Разбирает структуру листов стилей и диаграмм
NS = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main', 'c': 'http://schemas.openxmlformats.org/drawingml/2006/chart'}	# Задает пространства имен стандартных элементов Excel
NAMES = ['Верхняя шейка, верхняя часть (1)', 'Верхняя шейка, нижняя часть (2)', 'Бочка (3)', 'Нижняя шейка, верх (4)', 'Нижняя шейка, низ (5)']	# Сопоставляет актуальные подписи пяти участков
exe, folder = sys.argv[1:]	# Получает исполняемый файл и отдельный каталог проверки
folder = pathlib.Path(folder)	# Ограничивает создаваемые данные каталогом сборки
subprocess.run([exe, str(folder)], check=True)	# Создает книги через настоящий интерфейс C++
def rounded(value):	# Повторяет только правило представления чисел пользователя
    return math.copysign(math.floor(abs(value) * 10000 + 0.5) / 10000, value)	# Округляет положительные и отрицательные числа одинаковым правилом
def same(actual, expected):	# Сравнивает число с физическим результатом до округления
    assert isinstance(actual, float) and abs(actual - rounded(expected)) < 1e-8, (actual, expected)	# Исключает текст вместо числа и неверные единицы
def interpolate(samples, time, index):	# Независимо проверяет получасовые значения кривых
    times = [row[0] for row in samples]; right = bisect.bisect_left(times, time)	# Находит точные границы интерполяции
    if right == 0: return samples[0][index]	# Сохраняет начальное состояние без экстраполяции
    if right == len(samples): return samples[-1][index]	# Сохраняет фактический последний кадр
    left, after = samples[right - 1], samples[right]; weight = (time - left[0]) / (after[0] - left[0])	# Вычисляет вес по исходным временным меткам
    return left[index] * (1 - weight) + after[index] * weight	# Не использует функцию интерполяции самого экспортера
for case in ['default', 'irregular', 'partial']:	# Проверяет завершенную неполную и нерегулярную истории
    expected = json.loads((folder / (case + '.json')).read_text())	# Читает независимый снимок результата ядра
    with zipfile.ZipFile(folder / (case + '.xlsx')) as archive:	# Открывает фактически сохраненный файл Excel
        assert archive.testzip() is None	# Исключает ошибки сжатия и контрольных сумм
        xml = lambda path: ET.fromstring(archive.read(path))	# Разбирает только известные элементы формата
        workbook = xml('xl/workbook.xml')	# Получает настоящие имена листов книги
        assert [s.attrib['name'] for s in workbook.findall('s:sheets/s:sheet', NS)] == ['Параметры', 'Объем, м3', 'Толщина, мм']	# Требует ровно три согласованных листа
        strings = [''.join(node.itertext()) for node in xml('xl/sharedStrings.xml').findall('s:si', NS)]	# Восстанавливает русские подписи общей таблицы строк
        styles = xml('xl/styles.xml'); formats = {n.attrib['numFmtId']: n.attrib['formatCode'] for n in styles.findall('s:numFmts/s:numFmt', NS)}	# Находит пользовательский формат четырех знаков
        cell_styles = styles.find('s:cellXfs', NS)	# Связывает числовые ячейки с форматом отображения
        pages = []	# Хранит проверенные значения трех листов
        for page_index in range(1, 4):	# Проверяет каждый лист независимо от отображения
            cells = {}	# Собирает только заполненные ячейки текущего листа
            for cell in xml(f'xl/worksheets/sheet{page_index}.xml').findall('.//s:sheetData/s:row/s:c', NS):	# Читает значения без стороннего Excel парсера
                value = cell.find('s:v', NS)	# Получает сохраненное числовое или строковое значение
                if value is None: continue	# Пропускает пустые объединенные ячейки и границы
                if cell.attrib.get('t') == 's': cells[cell.attrib['r']] = strings[int(value.text)]	# Восстанавливает текст без потери кириллицы
                else:	# Проверяет физические числовые значения отдельно от строк
                    number = float(value.text); assert math.isfinite(number)	# Запрещает недопустимые результаты в готовой книге
                    code = formats[cell_styles[int(cell.attrib['s'])].attrib['numFmtId']]	# Отличает числовой показатель от длительности Excel
                    if code != '[hh]:mm:ss': assert abs(number - rounded(number)) < 1e-8 and code == '0.0000'	# Требует четыре знака только у обычных чисел
                    cells[cell.attrib['r']] = number	# Сохраняет число для дальнейшей физической сверки
            pages.append(cells)	# Фиксирует данные проверенного листа
        same(pages[0]['D10'], expected['volume']); same(pages[0]['B10'], 1022)	# Проверяет общий объем и наружный диаметр формы
        assert xml('xl/worksheets/sheet1.xml').find('.//s:pane', NS) is None	# Не закрепляет строки и столбцы параметров
        for page_index in [2, 3]:	# Проверяет закрепление только на листах результатов
            pane = xml(f'xl/worksheets/sheet{page_index}.xml').find('.//s:pane', NS)	# Читает настоящее определение закрепленной области
            assert pane.attrib.get('ySplit') == '5' and not pane.attrib.get('xSplit')	# Сохраняет пять строк без закрепления первого столбца
        assert [pages[0][f'{c}4'] for c in 'ABCD'] == ['Участок', 'Диаметр, мм', 'Длина, мм', 'Объем, м3']	# Исключает прежние столбцы первой таблицы параметров
        assert not any(value in ['Числовой горизонт .roll', 'DS', 'SC', 'SK'] for value in pages[0].values())	# Требует полные названия без служебных полей
        coating_row = next(key[1:] for key, value in pages[0].items() if value == 'Слой краски (SC)')	# Находит покрытие по полному названию параметра
        same(pages[0]['C' + coating_row], expected['coating'])	# Сверяет числовое значение покрытия текущего проекта
        blocks = []	# Запоминает реальные начала отдельных таблиц участков
        for sheet_index, unit in [(1, 'м3'), (2, 'мм')]:	# Проверяет обе системы единиц геометрических слоев
            cells = pages[sheet_index]; starts = []	# Собирает положения заголовков по именам участков
            for index, section in enumerate(expected['sections']):	# Проверяет полную историю каждого участка отдельно
                candidates = [int(key[1:]) for key, value in cells.items() if key.startswith('A') and value == NAMES[index] and cells.get('A' + str(int(key[1:]) + 1)) == 'Участок']	# Находит заголовок без зависимости от длины истории
                assert len(candidates) == 1; start = candidates[0]; starts.append(start)	# Требует одну отдельную таблицу данного участка
                assert [cells[f'{c}{start + 1}'] for c in 'ABCDE'] == ['Участок', 'Время, с', f'Твердый слой, {unit}', f'Двухфазный слой, {unit}', f'Жидкий слой, {unit}']	# Проверяет состав столбцов без повторенного времени затвердевания
                assert cells[f'G{start}'] == 'Время затвердевания, с'	# Размещает событие после пустого столбца справа
                if section['completed'] is None: assert cells[f'G{start + 1}'] == 'Не достигнуто'	# Сохраняет явное отсутствие события
                else: assert abs(cells[f'G{start + 1}'] * 86400 - math.floor(section['completed'] + 0.5)) < 1e-7	# Проверяет числовую длительность с округлением секунды
                for offset, sample in enumerate(section['samples']):	# Сверяет каждый исходный расчетный кадр
                    row = start + 2 + offset; assert cells[f'A{row}'] == NAMES[index]	# Исключает смешение участков в отдельной таблице
                    same(cells[f'B{row}'], sample[0])	# Сохраняет секунды в полной истории
                    for z, letter in enumerate('CDE'): same(cells[f'{letter}{row}'], sample[1 + z + (3 if sheet_index == 2 else 0)])	# Независимо сверяет объемы и радиальные толщины
                    assert f'F{row}' not in cells and f'H{row}' not in cells	# Сохраняет разделяющие пустые столбцы между областями
                assert f"A{start + 2 + len(section['samples'])}" not in cells	# Отделяет соседние таблицы пустой строкой
            blocks.append(starts)	# Передает расположение таблиц проверке диаграмм
        for drawing_index in [1, 2]:	# Проверяет расположение объектов рядом со своими таблицами
            drawing = xml(f'xl/drawings/drawing{drawing_index}.xml')	# Читает сохраненные якоря настоящих диаграмм Excel
            ns = '{http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing}'	# Задает пространство имен расположения объектов
            anchors = drawing.findall(ns + 'twoCellAnchor/' + ns + 'from')	# Находит верхние левые углы пяти диаграмм
            positions = [(int(a.find(ns + 'col').text), int(a.find(ns + 'row').text)) for a in anchors]	# Получает столбец и строку каждого объекта
            assert positions == [(8, start - 1) for start in blocks[drawing_index - 1]]	# Требует график после двух пустых разделителей
        charts = sorted((n for n in archive.namelist() if re.fullmatch(r'xl/charts/chart\d+\.xml', n)), key=lambda n: int(re.search(r'(\d+)\.xml', n).group(1)))	# Получает десять встроенных диаграмм в порядке создания
        assert len(charts) == 10	# Требует пять диаграмм на каждом листе результатов
        for index, path in enumerate(charts):	# Проверяет каждую диаграмму и источник ее чисел
            chart = xml(path); section = expected['sections'][index % 5]; samples = section['samples']	# Находит независимую историю нужного участка
            sheet_index = 1 if index < 5 else 2; cells = pages[sheet_index]	# Выбирает физическую размерность данной диаграммы
            labels = ''.join(chart.itertext())	# Читает заголовки и подписи сохраненных объектов
            assert NAMES[index % 5] in labels and 'Время, мин' in labels	# Требует корректное имя участка и ось минут
            assert ('Объем, м3' if sheet_index == 1 else 'Толщина, мм') in labels	# Проверяет вертикальную единицу измерения
            assert chart.find('.//c:scatterChart', NS) is not None	# Требует настоящую числовую временную ось
            assert any(float(node.attrib['val']) == 30 for node in chart.findall('.//c:majorUnit', NS))	# Проверяет получасовой шаг делений времени
            duration = samples[-1][0]; target = []	# Формирует независимую сетку графиков
            t = 0	# Начинает график в момент окончания заполнения
            while t < duration: target.append(t); t += 1800	# Добавляет только получасовые моменты
            target.append(duration)	# Сохраняет последний неполный интервал без потери кадра
            series = chart.findall('.//c:scatterChart/c:ser', NS); assert len(series) == 3	# Требует три фазовые кривые каждого участка
            for line, zone in zip(series, [2, 1, 0]):	# Сверяет порядок жидкого двухфазного и твердого слоев
                xref = line.find('c:xVal/c:numRef/c:f', NS).text; yref = line.find('c:yVal/c:numRef/c:f', NS).text	# Получает реальные адреса данных серий
                ranges = []	# Разбирает оба диапазона без догадок о строках
                for ref in [xref, yref]:	# Проверяет адреса времени и соответствующего слоя
                    match = re.fullmatch(r"'([^']+)'!\$([A-Z]+)\$(\d+):\$([A-Z]+)\$(\d+)", ref); assert match, ref	# Требует корректную ссылку на именованный лист
                    assert match[1] == ['Параметры', 'Объем, м3', 'Толщина, мм'][sheet_index] and match[2] == match[4]	# Исключает кривую с другого листа или столбца
                    ranges.append([cells[f'{match[2]}{r}'] for r in range(int(match[3]), int(match[5]) + 1)])	# Читает именно связанные с диаграммой ячейки
                assert len(ranges[0]) == len(target)	# Исключает перенос всех тридцатисекундных кадров на график
                for t, x, y in zip(target, *ranges):	# Сверяет каждый графический момент и физическую величину
                    same(x, t / 60); same(y, interpolate(samples, t, 1 + zone + (3 if sheet_index == 2 else 0)))	# Проверяет последнюю точку и независимую интерполяцию
    print(case + ': структура, все кадры и десять графиков проверены')	# Сообщает короткий результат каждого проверочного сценария
