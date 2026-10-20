# Tendril — randomized sampler (VST3)

Каждая нота запускает случайный сэмпл из загруженных. Аналог VINE по функциям.

## Сборка под Windows (FL Studio)
**Вариант 1 — без установки компилятора:** залей папку на GitHub → вкладка Actions → "Build Tendril VST3" → скачай артефакт `Tendril.vst3`.
**Вариант 2 — локально:** Visual Studio 2022 (Desktop C++) + CMake:
    cmake -B build -A x64
    cmake --build build --config Release --target Tendril_VST3
Результат: build/Tendril_artefacts/Release/VST3/Tendril.vst3
Скопируй в `C:\Program Files\Common Files\VST3`, в FL Studio: Options → Manage plugins → Find plugins.

## Управление
MUTATE — случайная стартовая позиция (0% = с начала) · SPREAD — случайные пан и детюн · COLOR — LP/HP фильтр
BLOOM — реверб · TAIL — release · SPROUT — доп. слой на октаву выше · LOCK KEY — сэмплы без транспонирования
В списке: ON/OFF, ONE-SHOT/LOOP, зона клавиш (ANY / C3..C7), октава, X. Файлы и папки можно перетащить в окно.
JUCE — GPLv3/коммерческая лицензия.
