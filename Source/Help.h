#pragma once
#include <juce_core/juce_core.h>

// Hints and tour texts in English and Russian. The interface itself is always in English.
namespace Help
{
    enum class Lang { en, ru };

    struct Entry { const char* key; const char* en; const char* ru; };

    inline const Entry tips[] = {
        { "low", "LOW: optical compression that evens out the low end (up to 2.5 dB, level stays the same), transformer and tape density, a touch of tube and the Pultec low-end trick. Drag up/down, Shift = fine, double-click = zero.",
                 "LOW: оптическая компрессия выравнивает низ (до 2,5 дБ, громкость не меняется), плотность трансформатора и ленты, капля лампы и пультековский трюк на низу. Тяни вверх-вниз, Shift = точно, двойной клик = ноль." },
        { "lowMid", "LOW-MID: tube warmth, gentle optical compression (up to 1.5 dB), a little lift of quiet details and a soft Pultec cut of the mud.",
                    "LOW-MID: тепло лампы, мягкая оптическая компрессия (до 1,5 дБ), немного подъёма тихих деталей и мягкий пультековский срез мути." },
        { "highMid", "HIGH-MID: presence. Quiet details come forward (Dolby A style lift), loud peaks stay put, plus a little tube and a wide Pultec bell.",
                     "HIGH-MID: присутствие. Тихие детали выходят вперёд (подъём в стиле Dolby A), громкие пики не растут, плюс немного лампы и широкий пультековский колокол." },
        { "high", "HIGH: air. The quiet top opens up (breaths, tails, room), tape silk, and the Pultec air bell with a soft top.",
                  "HIGH: воздух. Тихий верх раскрывается (дыхание, хвосты, комната), шёлк ленты и пультековский колокол воздуха с мягким верхом." },
        { "screen", "The four bands. Drag the dots to move the crossovers, double-click a dot to reset it. Hover a band for S (solo) and M (mute).",
                    "Четыре полосы. Тяни точки, чтобы двигать границы, двойной клик по точке вернёт её на место. Наведи на полосу, и появятся S (соло) и M (мьют)." },
        { "solo", "Solo: hear only this band (you can solo several).", "Соло: слышно только эту полосу (можно включить на нескольких)." },
        { "mute", "Mute: silence this band.", "Мьют: заглушить эту полосу." },
        { "tube", "The tube glows as hard as Breathe works: the more you turn, the hotter it gets.",
                  "Лампа светится так же сильно, как работает Breathe: чем больше крутишь, тем сильнее она разогревается." },
        { "inMeter", "Input level.", "Уровень на входе." },
        { "outMeter", "Output level.", "Уровень на выходе." },
        { "ovs", "Oversampling: runs the saturation at a higher rate so the harmonics stay clean. More = cleaner top, more CPU. Lit when it is on.",
                 "Оверсэмплинг: сатурация считается на повышенной частоте, чтобы гармоники были чистыми. Больше = чище верх, но больше нагрузка. Светится, когда включён." },
        { "lin", "LIN: linear-phase crossovers and Pultec curve. Crystal clear for mastering, adds about 50 ms of latency (your DAW compensates it). Off: analog phase, no extra latency.",
                 "LIN: линейно-фазовые кроссоверы и кривая Пультека. Кристально чисто для мастеринга, даёт задержку около 50 мс (DAW её компенсирует). Выключено: аналоговая фаза, без задержки." },
        { "mix", "Mix: blend of the untouched and the processed sound.", "Mix: смесь чистого и обработанного звука." },
        { "age", "Age: how old and alive the unit is: drift, breathing harmonics, left/right differences, a hint of mains hum in the tubes.",
                 "Age: насколько прибор старый и живой: дрейф, дышащие гармоники, разница левого и правого канала, намёк на фон сети в лампах." },
        { "output", "Output level, -12 to +12 dB.", "Громкость на выходе, от -12 до +12 дБ." },
        { "in", "IN: how hard the signal hits the iron, -12 to +12 dB. The level is given back at the output, so only the colour changes. The ring shows the level: pale = too little, theme colour = sweet spot, red = too hot, turn it down a bit.",
                "IN: насколько сильно сигнал бьёт в железо, от -12 до +12 дБ. На выходе громкость возвращается обратно, меняется только окраска. Кольцо подсказывает уровень: бледное = мало, цвет темы = в самый раз, красное = перебор, убавь немного." },
        { "mode", "MASTER: delicate, for the whole mix (EQ up to about 2 dB). MIX: full strength for buses, stems and single tracks (EQ up to about half a real Pultec).",
                  "MASTER: деликатно, для всего микса (эквализация до ~2 дБ). MIX: полная сила для шин, стемов и отдельных дорожек (эквализация примерно до половины настоящего Пультека)." },
        { "bypass", "Bypass: the sound goes around the processing. The latency stays the same, so nothing jumps in time.",
                    "Bypass: звук идёт мимо обработки. Задержка остаётся той же, поэтому ничего не прыгает по времени." },
        { "gear", "Settings: theme, help, help language, the tour and window size.", "Настройки: тема, подсказки, язык подсказок, обучение и размер окна." },
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
              "An analog color processor: four bands, one knob each. Behind every knob a whole little studio rack works at once: tube, tape, transformer, optical compression, a Dolby-style lift of quiet details and a Pultec curve. Use Next and Back, or Skip at any time.",
              "Добро пожаловать в Breathe",
              "Аналоговый процессор цвета: четыре полосы, по одной ручке на каждую. За каждой ручкой одновременно работает целая маленькая стойка железа: лампа, лента, трансформатор, оптическая компрессия, подъём тихих деталей в стиле Dolby и кривая Пультека. Листай кнопками «Дальше» и «Назад» или пропусти в любой момент." },
        { "knobs", "One knob per band", "Turning a knob scales the whole recipe of its band at once, in fixed proportions. The range is delicate, mastering-grade: even at maximum nothing buzzes. At zero the band is untouched. Shift = fine, double-click = zero.",
                   "Одна ручка на полосу", "Ручка масштабирует весь рецепт своей полосы сразу, в постоянной пропорции. Диапазон деликатный, мастеринговый: даже на максимуме ничего не звенит. На нуле полоса нетронута. Shift = точно, двойной клик = ноль." },
        { "low", "LOW", "Optical compression evens out the low end (up to 2.5 dB) while the level stays put, transformer and tape add density, the Pultec trick makes it bigger and cleaner.",
                 "LOW", "Оптическая компрессия выравнивает низ (до 2,5 дБ), а громкость остаётся на месте, трансформатор и лента добавляют плотности, пультековский трюк делает низ больше и чище." },
        { "lowMid", "LOW-MID", "Tube warmth and body, a gentle optical squeeze (up to 1.5 dB), a little lift of quiet parts and a soft cut of the mud.",
                    "LOW-MID", "Тепло и тело лампы, мягкий оптический прижим (до 1,5 дБ), немного подъёма тихого и лёгкий срез мути." },
        { "highMid", "HIGH-MID", "Presence: quiet details come forward, loud peaks don't grow, so nothing gets harsh.",
                     "HIGH-MID", "Присутствие: тихие детали выходят вперёд, а громкие пики не растут, поэтому ничего не колется." },
        { "high", "HIGH", "Air: breaths, tails and room open up, with tape silk and the Pultec air bell.",
                  "HIGH", "Воздух: раскрываются дыхание, хвосты и комната, плюс шёлк ленты и пультековский колокол воздуха." },
        { "screen", "The bands", "Drag the dots to move the crossovers. The band recipes follow them smoothly. Hover a band and S (solo) and M (mute) appear in the middle.",
                    "Полосы", "Тяни точки, чтобы двигать границы полос. Рецепты полос плавно следуют за ними. Наведи на полосу, и посередине появятся S (соло) и M (мьют)." },
        { "tube", "The tube", "It glows as hard as Breathe works and flickers like a real one. Age makes it more nervous.",
                  "Лампа", "Светится так сильно, как работает Breathe, и мерцает, как настоящая. Age делает её нервнее." },
        { "ovs", "Oversampling", "Click to choose 1x to 16x. Higher keeps the harmonics cleaner and costs more CPU. It lights up when it is on.",
                 "Оверсэмплинг", "Кликни и выбери от 1x до 16x. Больше = чище гармоники, но выше нагрузка. Загорается, когда включён." },
        { "lin", "Linear phase", "LIN lit: linear-phase crossovers and Pultec curve, perfect for mastering, about 50 ms of latency (the DAW compensates). Off: analog phase, no latency.",
                 "Линейная фаза", "LIN горит: линейно-фазовые кроссоверы и кривая Пультека, идеально для мастеринга, задержка около 50 мс (DAW её компенсирует). Выключено: аналоговая фаза, без задержки." },
        { "mode", "MASTER or MIX", "MASTER is delicate, made for the whole mix. MIX unleashes everything: much more harmonics, compression, lift and a deeper Pultec curve, for buses and single tracks.",
                  "MASTER или MIX", "MASTER деликатный, для всего микса. MIX выпускает всё на волю: гораздо больше гармоник, компрессии, подъёма и более глубокая кривая Пультека, для шин и отдельных дорожек." },
        { "in", "IN", "How hard the signal hits the iron. The level comes back at the output. Watch the ring: pale = too little, theme colour = sweet spot, red = turn it down.",
                "IN", "Насколько сильно сигнал бьёт в железо. На выходе громкость возвращается. Смотри на кольцо: бледное = мало, цвет темы = в самый раз, красное = убавь." },
        { "mix", "Mix", "Blend of the untouched and the processed sound.", "Mix", "Смесь чистого и обработанного звука." },
        { "age", "Age", "How old and alive the unit is: drift, breathing harmonics, left/right differences.",
                 "Age", "Насколько прибор старый и живой: дрейф, дышащие гармоники, разница каналов." },
        { "output", "Output", "Output level, -12 to +12 dB, for matching the level when you compare with Bypass.",
                    "Output", "Громкость на выходе, от -12 до +12 дБ, чтобы выровнять уровень при сравнении с Bypass." },
        { "bypass", "Bypass", "Compare with the original. The latency stays the same, so nothing jumps.",
                    "Bypass", "Сравни с оригиналом. Задержка остаётся той же, поэтому ничего не прыгает." },
        { "gear", "Settings", "Theme, help on or off, help language, this tour and window size. Drag the bottom-right corner to resize.",
                  "Настройки", "Тема, подсказки, язык подсказок, этот тур и размер окна. Окно тянется за правый нижний угол." },
        { "", "That's it!", "Hover over anything for a hint. Now let your mix breathe.",
              "Готово!", "Наведи мышку на что угодно, и появится подсказка. А теперь дай миксу подышать." },
    };
    inline constexpr int tourLength = (int) (sizeof (tour) / sizeof (tour[0]));

    inline juce::String ui (const char* en, const char* ru, Lang lang)
    {
        return juce::String::fromUTF8 (lang == Lang::ru ? ru : en);
    }
}
