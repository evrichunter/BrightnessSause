# Brightness Sause By Evric Hunter

Parallel brightness enhancer for vocals. JUCE 8 / C++17, VST3 + Standalone, **0 samples of latency**.

## Obtener el VST3 para FL Studio (Windows)
1. Subí esta carpeta a un repo de GitHub (el workflow `.github/workflows/build.yml` ya está).
2. Pestaña **Actions → Build VST3 (Windows) → Run workflow**.
3. Descargá el artifact `BrightnessSause-VST3` y copiá la carpeta `Brightness Sause By Evric Hunter.vst3`
   a `C:\Program Files\Common Files\VST3\`. En FL Studio: Options → Manage plugins → Find plugins.

Build local (Windows, Visual Studio 2022 + CMake):
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target BrightnessSause_VST3

## Tests del DSP (sin JUCE)
    cd Tests && g++ -O2 -std=c++17 -I../Source dsp_test.cpp -o dsp_test && ./dsp_test

## Parámetros (IDs estables)
bypass, excite, character, harmonics, air, presence, softness, brightness, lowcut, presfreq, presgain, airfreq, airgain, q, eqbypass, mix, output.
BRIGHTNESS = macro de high-shelf amplio (5 kHz, hasta +6 dB) dentro de la etapa Air.

## Limitaciones declaradas
- Sin oversampling (pedido de latencia cero): el aliasing del exciter se limita con filtrado de banda antes y después del waveshaper, no se elimina.
- El exciter normaliza el nivel de la banda alta antes de saturar (el efecto no desaparece con voces suaves) y se atenúa solo si la entrada ya es brillante; no hay limitador ni AGC global.
- El Air tiene una guarda de sibilancia suave (reduce hasta 50% el realce cuando domina la banda 5.5k+).
