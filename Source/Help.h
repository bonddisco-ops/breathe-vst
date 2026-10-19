#pragma once
#include <juce_core/juce_core.h>

// Hints and tour texts in English and Russian. The interface itself is always in English.
namespace Help
{
    enum class Lang { en, ru };

    struct Entry { const char* key; const char* en; const char* ru; };

    inline const Entry tips[] = {
        { "low",
          "LOW: optical compression (up to 3.5 dB GR in MASTER, 6 dB in MIX) with automatic gain compensation, flux-dependent transformer saturation, tape saturation and a triode stage. Pultec low shelf with a simultaneous cut around 200 Hz. Drag to adjust, Shift for fine control, double-click to reset.",
          "LOW: оптическая компрессия (до 3,5 дБ подавления в MASTER, до 6 дБ в MIX) с автоматической компенсацией уровня, насыщение трансформатора по магнитному потоку, ленточное насыщение и триодный каскад. Пультековская низкая полка с одновременным вырезом в районе 200 Гц. Перетаскивание — регулировка, Shift — точная настройка, двойной клик — сброс." },
        { "lowMid",
          "LOW-MID: triode stage with a dominant 2nd harmonic and an extended odd series, light tape saturation, optical compression (up to 2 dB GR in MASTER, 4 dB in MIX) and a broad Pultec-style cut at the band centre.",
          "LOW-MID: триодный каскад с доминирующей второй гармоникой и протяжённым нечётным рядом, лёгкое ленточное насыщение, оптическая компрессия (до 2 дБ в MASTER, до 4 дБ в MIX) и широкий пультековский вырез в центре полосы." },
        { "highMid",
          "HIGH-MID: Dolby A-style upward expansion of low-level detail (up to +3.5 dB in MASTER, +6 dB in MIX; peaks are not raised), triode and tape saturation, Pultec presence bell around 4.4 kHz.",
          "HIGH-MID: подъём тихих деталей в стиле Dolby A (до +3,5 дБ в MASTER, до +6 дБ в MIX; пики не поднимаются), триодное и ленточное насыщение, пультековский колокол присутствия в районе 4,4 кГц." },
        { "high",
          "HIGH: Dolby A-style lift of low-level high-frequency content (up to +4.5 dB in MASTER, +8 dB in MIX), tape saturation, Pultec air bell with a gentle high-shelf roll-off above 17 kHz.",
          "HIGH: подъём тихих высокочастотных составляющих в стиле Dolby A (до +4,5 дБ в MASTER, до +8 дБ в MIX), ленточное насыщение, пультековский колокол воздуха с мягким спадом полкой выше 17 кГц." },
        { "screen",
          "Spectrum and band split. Drag a marker to move a crossover (Linkwitz-Riley, 24 dB/oct), double-click to reset it. Hover a band to reveal Solo and Mute.",
          "Спектр и разделение на полосы. Перетаскивание маркера сдвигает кроссовер (Линквиц–Рили, 24 дБ/окт), двойной клик возвращает его. При наведении на полосу появляются Solo и Mute." },
        { "solo", "Solo: monitor this band only. Several bands can be soloed; while any Solo is active, Mute is ignored.",
                  "Solo: прослушивание только этой полосы. Можно включить на нескольких полосах; пока активно соло, Mute не действует." },
        { "mute", "Mute: remove this band from the output.", "Mute: исключение полосы из выходного сигнала." },
        { "tube", "Activity indicator: the filament brightness follows the amount of processing.",
                  "Индикатор активности: яркость нити накала соответствует интенсивности обработки." },
        { "inMeter", "Input peak level, dBFS.", "Пиковый уровень на входе, dBFS." },
        { "outMeter", "Output peak level, dBFS.", "Пиковый уровень на выходе, dBFS." },
        { "ovs", "Oversampling factor for the saturation stages (1x-16x). Higher factors reduce aliasing at the cost of CPU. Minimum-phase IIR filters with LIN off, linear-phase FIR with LIN on.",
                 "Кратность передискретизации для каскадов насыщения (1x–16x). Более высокая кратность снижает алиасинг ценой нагрузки на процессор. Без LIN — IIR-фильтры минимальной фазы, с LIN — FIR линейной фазы." },
        { "lin", "LIN: linear-phase crossovers (FIR, about 48 ms of latency at 48 kHz, compensated by the host). Off: minimum-phase crossovers with no added latency. The Pultec curve is always minimum phase.",
                 "LIN: линейно-фазовые кроссоверы (FIR, задержка около 48 мс при 48 кГц, компенсируется хостом). Выключено: кроссоверы минимальной фазы без дополнительной задержки. Пультековская кривая всегда минимально-фазовая." },
        { "mix", "Dry/wet balance. The dry path is latency- and phase-aligned, so blending causes no comb filtering.",
                 "Баланс сухого и обработанного сигнала. Сухой путь выровнен по задержке и фазе, поэтому смешивание не вызывает гребенчатой фильтрации." },
        { "age", "AGE: tape scrape flutter. Fast, noise-like amplitude modulation above roughly 500 Hz, decorrelated between channels and zero-mean. Modulation products below 200 Hz are filtered out.",
                 "AGE: скрейп-флаттер ленты. Быстрая шумовая амплитудная модуляция выше ~500 Гц, некоррелированная между каналами и с нулевым средним. Продукты модуляции ниже 200 Гц отфильтрованы." },
        { "ageOff", "AGE is bypassed while any band is soloed or muted, so the band can be judged without the flutter.",
                    "Пока включено Solo или Mute любой полосы, AGE отключён, чтобы флаттер не мешал оценивать полосу." },
        { "output", "Output gain, -30 to +30 dB.", "Выходное усиление, от -30 до +30 дБ." },
        { "in", "IN: gain into the saturation stages, -30 to +30 dB, compensated at the output. The ring shows the RMS level at the input of the stages: dim - below the working range, theme colour - working range (-24 to -12 dB RMS), red - overdriven.",
                "IN: усиление на входе каскадов насыщения, от -30 до +30 дБ, с компенсацией на выходе. Кольцо показывает RMS-уровень на входе каскадов: бледное — ниже рабочего диапазона, цвет темы — рабочий диапазон (от -24 до -12 дБ RMS), красное — перегрузка." },
        { "mode", "MASTER: mastering-grade intensity (Pultec curve up to about 2 dB, low single-digit THD). MIX: full intensity for buses, stems and tracks (curve up to about 7 dB, several times more compression, lift and harmonics).",
                  "MASTER: мастеринговая интенсивность (пультековская кривая до ~2 дБ, искажения единицы процентов). MIX: полная интенсивность для шин, стемов и дорожек (кривая до ~7 дБ, компрессия, подъём и гармоники в разы сильнее)." },
        { "bypass", "Bypass with latency preserved: switching is time-aligned and click-free.",
                    "Bypass с сохранением задержки: переключение выровнено по времени и происходит без щелчков." },
        { "gear", "Settings: theme, hints, hint language, tour, window size.", "Настройки: тема, подсказки, язык подсказок, обучающий тур, размер окна." },
        { "settings", "Settings: theme, hints, hint language, tour, window size, about.", "Настройки: тема, подсказки, язык подсказок, обучающий тур, размер окна, о плагине." },
        { "power", "Power: processing on or off (latency preserved, so switching is time-aligned).", "Power: обработка включена или выключена (задержка сохраняется, переключение выровнено по времени)." },
        { "undo", "Undo the last change.", "Отменить последнее изменение." },
        { "redo", "Redo the change that was undone.", "Вернуть отменённое изменение." },
        { "slots", "Settings slots A-D for quick comparison. Each slot keeps its own complete settings.", "Слоты настроек A–D для быстрого сравнения. Каждый слот хранит свои полные настройки." },
        { "copy", "Copy all settings of the current slot.", "Скопировать все настройки текущего слота." },
        { "paste", "Paste the copied settings into the current slot.", "Вставить скопированные настройки в текущий слот." },
        { "reset", "Reset every parameter to its default.", "Сбросить все параметры к значениям по умолчанию." },
        { "autoOut", "AUTO OUT: the output loudness follows the input loudness (K-weighted, slow), so comparisons with Power off are level-matched.", "AUTO OUT: громкость выхода следует за громкостью входа (взвешенно по слуху, медленно), сравнение с выключенной обработкой выровнено по уровню." },
        { "autoIn", "AUTO IN: the drive into the saturation stages is set automatically to the working level (about -18 dB RMS) whatever the track level. IN becomes an offset from there.", "AUTO IN: уровень на входе каскадов насыщения выставляется автоматически на рабочий (около -18 дБ RMS) при любом уровне трека. IN становится смещением относительно него." },
        { "monitor", "Listen: Mid, Side, Left or Right alone, or the mono sum, always in the centre. Monitoring only.", "Прослушивание: только Mid, Side, левый или правый канал, либо моно-сумма, всегда по центру. Только мониторинг." },
        { "procMode", "Channel mode: the two sides are Mid / Side or Left / Right.", "Режим каналов: две стороны обработки — Mid / Side или Left / Right." },
        { "side", "The side the band knobs and Advanced trims edit. With Link on both sides move together.", "Сторона, которую редактируют ручки полос и регуляторы Advanced. С Link обе стороны двигаются вместе." },
        { "link", "Link: both sides share their settings. Grabbing a control of one side hands all its settings to the other.", "Link: обе стороны используют одни настройки. Взявшись за регулятор одной стороны, вы передаёте все её настройки другой." },
        { "split", "Split: show both sides at once, a row of band knobs per side.", "Split: показать обе стороны сразу, по ряду ручек полос на каждую." },
        { "logo", "About Breathe: version and system information.", "О плагине: версия и системная информация." },
        { "adv", "ADVANCED: fine control over each band's recipe. Lit while at least one band is active; trims act only on active bands.",
                 "ADVANCED: точная настройка рецепта каждой полосы. Подсвечивается, когда активна хотя бы одна полоса; регуляторы действуют только на активные полосы." },
        { "advScreen", "EQ: resulting Pultec curve over the output spectrum; band brightness follows the drive into the saturation stages. DYN: the selected band's compressor (waveform, threshold, gain reduction) or Dolby A-style lift (band level over the last 4 s before and after; the filled gap is what the lift brings up). With Link off the screen shows the side being edited. The screen switches to DYN while a COMP or LIFT trim is adjusted.",
                       "EQ: итоговая пультековская кривая поверх спектра выхода; яркость полосы соответствует драйву каскадов насыщения. DYN: компрессор выбранной полосы (осциллограмма, порог, подавление) или подъём в стиле Dolby A (уровень полосы за последние 4 с до и после; заливка между ними — то, что поднял лифт). С выключенным Link экран показывает редактируемую сторону. Экран переключается на DYN при регулировке COMP или LIFT." },
        { "advEq", "EQ trim: depth of the band's Pultec section. Centre = factory recipe, -100 % = bypassed, +100 % = full depth of a hardware Pultec. Shift for 0.1 % steps, double-click to centre.",
                   "EQ: глубина пультековской секции полосы. Центр — заводской рецепт, -100 % — секция выключена, +100 % — полная глубина аппаратного Pultec. Shift — шаг 0,1 %, двойной клик — центр." },
        { "advDrive", "DRIVE trim: drive into the band's tube, tape and transformer stages. Centre = factory recipe, -100 % = no saturation, +100 % = +12 dB of drive. Output level stays compensated.",
                      "DRIVE: уровень драйва триодного, ленточного и трансформаторного каскадов полосы. Центр — заводской рецепт, -100 % — без насыщения, +100 % — +12 дБ драйва. Выходной уровень остаётся скомпенсированным." },
        { "advComp", "COMP trim: threshold of the optical compressor relative to the factory setting. + lowers it (up to -12 dB, with up to twice the gain reduction), - raises it (up to +12 dB). Attack and release keep their optical behaviour. The threshold line on the DYN screen can also be dragged.",
                     "COMP: порог оптического компрессора относительно заводского. «+» опускает его (до -12 дБ, подавление до двух раз глубже), «-» поднимает (до +12 дБ). Атака и восстановление сохраняют оптический характер. Линию порога на экране DYN можно также перетаскивать мышью." },
        { "advLift", "LIFT trim: amount of the Dolby A-style low-level lift. Centre = factory recipe, -100 % = off, +100 % = double amount.",
                     "LIFT: величина подъёма тихих деталей в стиле Dolby A. Центр — заводской рецепт, -100 % — выключен, +100 % — двойная величина." },
        { "advReset", "Return every Advanced trim to the factory recipe.", "Вернуть все регуляторы Advanced к заводскому рецепту." },
    };

    inline juce::String tip (const char* key, Lang lang)
    {
        for (const auto& e : tips)
            if (juce::String (e.key) == key)
                return juce::String::fromUTF8 (lang == Lang::ru ? e.ru : e.en);
        return {};
    }

    struct Step { const char* target; const char* titleEn; const char* textEn; const char* titleRu; const char* textRu; };

    inline const Step tour[] = {
        { "", "Welcome to Breathe",
              "A four-band analog colour processor. Each band knob scales a fixed recipe of triode, tape and transformer saturation, optical compression, Dolby A-style low-level lift and a Pultec-style curve. Use Next and Back, or Skip at any time.",
              "Добро пожаловать в Breathe",
              "Четырёхполосный аналоговый процессор цвета. Ручка каждой полосы масштабирует фиксированный рецепт: триодное, ленточное и трансформаторное насыщение, оптическую компрессию, подъём тихих деталей в стиле Dolby A и пультековскую кривую. Листайте кнопками «Дальше» и «Назад» или пропустите тур." },
        { "knobs", "One knob per band", "All processes of a band are scaled together in fixed proportions. At zero the band is bit-transparent. Shift for fine control, double-click to reset.",
                   "Одна ручка на полосу", "Все процессы полосы масштабируются одновременно в постоянной пропорции. На нуле полоса прозрачна бит-в-бит. Shift — точная настройка, двойной клик — сброс." },
        { "low", "LOW", "Optical compression with automatic gain compensation, flux-dependent transformer saturation, tape saturation and a Pultec low shelf with a cut around 200 Hz.",
                 "LOW", "Оптическая компрессия с компенсацией уровня, насыщение трансформатора по магнитному потоку, ленточное насыщение и пультековская низкая полка с вырезом в районе 200 Гц." },
        { "lowMid", "LOW-MID", "Triode stage with a dominant 2nd harmonic, light optical compression and a broad Pultec-style cut at the band centre.",
                    "LOW-MID", "Триодный каскад с доминирующей второй гармоникой, мягкая оптическая компрессия и широкий пультековский вырез в центре полосы." },
        { "highMid", "HIGH-MID", "Dolby A-style lift of low-level detail with peaks left untouched, triode and tape saturation, Pultec presence bell.",
                     "HIGH-MID", "Подъём тихих деталей в стиле Dolby A без подъёма пиков, триодное и ленточное насыщение, пультековский колокол присутствия." },
        { "high", "HIGH", "Dolby A-style lift of low-level high-frequency content, tape saturation and a Pultec air bell with a soft top shelf.",
                  "HIGH", "Подъём тихих высокочастотных составляющих в стиле Dolby A, ленточное насыщение и пультековский колокол воздуха с мягкой верхней полкой." },
        { "screen", "Band split", "Drag the markers to move the Linkwitz-Riley crossovers. The band recipes follow the band centres smoothly. Hover a band for Solo and Mute.",
                    "Разделение на полосы", "Перетаскивайте маркеры, чтобы сдвигать кроссоверы Линквица–Рили. Рецепты полос плавно следуют за центрами полос. При наведении на полосу появляются Solo и Mute." },
        { "tube", "Activity indicator", "The filament brightness follows the amount of processing.",
                  "Индикатор активности", "Яркость нити накала соответствует интенсивности обработки." },
        { "ovs", "Oversampling", "Choose 1x to 16x for the saturation stages. Higher factors reduce aliasing and increase CPU load.",
                 "Передискретизация", "Выбор кратности от 1x до 16x для каскадов насыщения. Более высокая кратность снижает алиасинг и увеличивает нагрузку." },
        { "lin", "Linear phase", "LIN on: linear-phase FIR crossovers, about 48 ms of latency compensated by the host. Off: minimum-phase crossovers, no added latency.",
                 "Линейная фаза", "LIN включён: линейно-фазовые FIR-кроссоверы, задержка около 48 мс компенсируется хостом. Выключен: кроссоверы минимальной фазы без дополнительной задержки." },
        { "mode", "MASTER or MIX", "MASTER: mastering-grade intensity. MIX: full intensity for buses, stems and individual tracks.",
                  "MASTER или MIX", "MASTER: мастеринговая интенсивность. MIX: полная интенсивность для шин, стемов и отдельных дорожек." },
        { "in", "IN", "Gain into the saturation stages, compensated at the output. The ring indicates the working range: dim - too low, theme colour - optimal, red - overdriven.",
                "IN", "Усиление на входе каскадов насыщения с компенсацией на выходе. Кольцо показывает рабочий диапазон: бледное — мало, цвет темы — оптимально, красное — перегрузка." },
        { "mix", "Mix", "Dry/wet balance with a latency- and phase-aligned dry path.", "Mix", "Баланс сухого и обработанного сигнала; сухой путь выровнен по задержке и фазе." },
        { "age", "Age", "Tape scrape flutter above roughly 500 Hz. The low end is not affected.",
                 "Age", "Скрейп-флаттер ленты выше ~500 Гц. Низкие частоты не затрагиваются." },
        { "output", "Output", "Output gain, -30 to +30 dB, for level matching against Power off.",
                    "Output", "Выходное усиление от -30 до +30 дБ для выравнивания громкости при сравнении с выключенным Power." },
        { "adv", "Advanced", "Opens the Advanced panel: per-band EQ, DRIVE, COMP and LIFT trims around the factory recipe, with an analyzer showing the resulting curve.",
                 "Advanced", "Открывает панель Advanced: регуляторы EQ, DRIVE, COMP и LIFT для каждой полосы относительно заводского рецепта и анализатор с итоговой кривой." },
        { "bypass", "Power", "Compare with the original. Latency is preserved, so the switch is time-aligned. The rest of the top bar: undo / redo, A-D settings slots, copy / paste, reset, AUTO OUT / AUTO IN, listen M S L R mono, M/S or L/R, side select, Link and Split.",
                    "Power", "Сравнение с исходным сигналом. Задержка сохраняется, поэтому переключение выровнено по времени. Остальная верхняя панель: отмена / повтор, слоты A-D, копирование / вставка, сброс, AUTO OUT / AUTO IN, прослушивание M S L R mono, M/S или L/R, выбор стороны, Link и Split." },
        { "gear", "Settings", "Theme, hints, hint language, this tour and window size. The window can also be resized from the bottom-right corner.",
                  "Настройки", "Тема, подсказки, язык подсказок, этот тур и размер окна. Размер окна также меняется за правый нижний угол." },
        { "", "Done", "Hover any control for a description.",
              "Готово", "Наведите курсор на любой элемент, чтобы увидеть его описание." },
    };
    inline constexpr int tourLength = (int) (sizeof (tour) / sizeof (tour[0]));

    inline juce::String ui (const char* en, const char* ru, Lang lang)
    {
        return juce::String::fromUTF8 (lang == Lang::ru ? ru : en);
    }
}
