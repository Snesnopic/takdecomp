# takdecomp

`takdecomp` è una suite open source in C++20 per la decodifica e codifica del formato audio lossless **TAK** (Tom's lossless Audio Kompressor). 
Progettato per offrire piena compatibilità cross-platform con gli eseguibili originali `takc.exe` e `takd.exe`, offre una base di codice pulita, facilmente integrabile, multithread e completamente indipendente da Windows.

## Funzionalità

- **Decodifica completa (`takdec`)**: Riproduzione 1:1 del bitstream in WAV, estrazione MD5, decodifica APEv2.
- **Codifica ad alte prestazioni (`takenc`)**: Compressione audio in frame, supporto multithread (`-tn#`), APEv2 Tagging, wave metadata extraction e bitstream verify.
- **Supporto multi-piattaforma**: Testato e nativamente compilabile su Windows (x64, x86, ARM64), Linux e macOS.
- **Costruito come libreria**: Struttura C++ modulare per integrare encoder o decoder nel proprio ecosistema software (player, converter, DAWs).

## Compilazione

Il progetto usa CMake. Puoi compilare facilmente da riga di comando:

```bash
git clone https://github.com/tuo-utente/takdecomp.git
cd takdecomp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Questo produrrà gli eseguibili `takenc` e `takdec` all'interno della cartella `bin/`.

## Utilizzo degli eseguibili (CLI)

La sintassi e i flag replicano fedelmente gli eseguibili TAK originali.

### Decodifica (takdec)

```bash
./takdec input.tak [output.wav] [opzioni]
```
- `-t`: Esegue solo il test di integrità del file (non scrive l'output audio).
- `-md5`: Calcola e verifica l'MD5 del bitstream rispetto a quello contenuto nell'header.
- Se l'output non è specificato, il file `.wav` verrà salvato nella stessa cartella con lo stesso nome.

### Codifica (takenc)

```bash
./takenc input.wav [output.tak] [opzioni]
```
- `-p#`: Preset di compressione (da 0 a 5). P5 = massima compressione, P0 = massima velocità. Extra: -p#m, -p#e per massimo effort.
- `-tn#`: Specifica il numero di thread da usare (default: 1).
- `-tt "Key=Value"`: Scrive tag APEv2 nel file in output.
- `-wm#`: Scrive i metadata raw del file wave (0 = ignora, 1 = copia i foreign chunks, default: 1).
- `-ihs`: Ignora la dimensione dell'header (utile in pipe con stream di lunghezza non definita a priori).
- `-overwrite`: Sovrascrive il file di destinazione in modo silente.
- `-v`: Verifica l'integrità dei frame richiamando automaticamente la decodifica post-encode.

## Utilizzo come libreria

Entrambi gli engine sono racchiusi nei target statici `takdec_core` e `takenc_core`.
Aggiungendolo al tuo progetto tramite CMake:

```cmake
add_subdirectory(takdecomp)
target_link_libraries(tuo_eseguibile PRIVATE takdec_core takenc_core)
```

Esempio base di decodifica:
```cpp
#include "tak_decoder/decoder.hpp"
#include <iostream>

int main() {
    try {
        takdecomp::DecodeResult res = takdecomp::Decoder::decode_file("audio.tak", "audio.wav");
        std::cout << "Decoded " << res.samples_decoded << " samples!\n";
    } catch (const std::exception& e) {
        std::cerr << "Errore: " << e.what() << "\n";
    }
}
```

Esempio base di codifica:
```cpp
#include "tak_encoder/encoder.hpp"

int main() {
    takenc::EncoderConfig cfg;
    cfg.preset = 2; // preset normale
    cfg.threads = 4; // usa 4 threads

    takenc::EncodeResult res = takenc::Encoder::encode_file("audio.wav", "audio.tak", cfg, nullptr);
    return 0;
}
```

## Licenza

Questo progetto è rilasciato nei termini della licenza specificata nel repository. L'implementazione dell'algoritmo di compressione / decompressione si basa sulle specifiche del formato aperto Tom's lossless Audio Kompressor.
