# PulseDeck

Оригинальный VST3-инструмент в духе «плагин с готовыми звуками + крутилки эффектов + MIDI-клавиатура».
Идея вдохновлена описанием Shroomdeck, но **весь код, звуки и интерфейс здесь свои**: ничего не скопировано.

## Что внутри

- **200 звуков**, которые генерирует код при запуске (10 категорий × 20 вариантов: Pluck, Bell, EPiano, Pad, Bass, Lead, Mallet, Kick, Texture, Choir). Никаких чужих сэмплов.
- **15 ручек эффектов:** Drive, Bitcrush, Downsample, LP Cutoff, HP Cutoff, Resonance, Chorus, Chorus Rate, Delay, Delay Time, Feedback, Reverb, Reverb Size, Width, Tremolo.
- Ещё 4 ручки: Pitch, Attack, Release, Master.
- 16 голосов, экранная MIDI-клавиатура, приём MIDI от хоста и контроллеров.
- Свои пресеты: кнопка **Save preset** сохраняет все ручки в файл `*.pdpreset` (папка `PulseDeck/Presets` в данных приложений пользователя). Состояние также сохраняется внутри проекта DAW.
- Форматы: VST3 и Standalone (Windows / macOS / Linux).

## Сборка

Нужны CMake 3.22+ и компилятор C++17 (Visual Studio 2022 / Xcode / gcc). JUCE скачается сам при первой конфигурации.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Готовый плагин появится в `build/PulseDeck_artefacts/Release/VST3/`. Скопируйте папку `PulseDeck.vst3` в:

- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `~/Library/Audio/Plug-Ins/VST3`

Перед публикацией замените `COMPANY_NAME` и коды плагина в `CMakeLists.txt`.

## Проверка звукового ядра без JUCE

Ядро (`Source/Core`) не зависит от JUCE:

```bash
g++ -std=c++17 -O2 -Wall -Wextra tests/core_test.cpp -o core_test
./core_test demo.wav
```

Тест проверяет все 200 звуков на 44.1 / 48 / 96 кГц, работу каждой ручки, вытеснение голосов и пишет `demo.wav` для прослушивания.

## Структура

```
Source/Core/SoundBank.h   генератор 200 звуков
Source/Core/Engine.h      голоса + цепочка эффектов
Source/PluginProcessor.*  параметры, MIDI, состояние (JUCE)
Source/PluginEditor.*     интерфейс (JUCE)
tests/core_test.cpp       тест ядра
```

## Что сделано и чего нет

- Ядро собрано и проверено тестами. **Обёртка на JUCE (Processor/Editor) написана, но в этой среде не компилировалась**, потому что JUCE нельзя было скачать. Возможны мелкие правки при первой сборке.
- Звуки одноразовые, без зацикливания: пэды и хор затухают сами примерно через 3 секунды.
- Защиты от копирования нет: плагин открытый, она ему не нужна.
- Загрузка плагина занимает около секунды, пока генерируются звуки.

## Лицензии

Код PulseDeck распространяется под MIT (файл `LICENSE`). JUCE имеет собственную лицензию (AGPLv3 или коммерческую): если планируете продавать плагин без открытия исходников, нужна коммерческая лицензия JUCE. Название «VST» — торговая марка Steinberg.
