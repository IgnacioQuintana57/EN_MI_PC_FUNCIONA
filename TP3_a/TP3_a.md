# TP3 a

## Integrantes
- Antonino, Tadeo - tadeo.antonino@mi.unc.edu.ar
- Fioramonti, Martino - martino.fioramonti@mi.unc.edu.ar
- Quintana, Ignacio Agustin - ignacio.agustin.quintana@mi.unc.edu.ar

---

## Repositorio GitHub

https://github.com/IgnacioQuintana57/EN_MI_PC_FUNCIONA


---

## Introducción

Objetivo General: Comprender la arquitectura de la Interfaz de Firmware Extensible Unificada (UEFI) como un entorno pre-sistema operativo, desarrollar binarios nativos, entender su formato y ejecutar rutinas tanto en entornos emulados como en hardware físico (bare metal).

---

### Trabajo Práctico 1: Exploración del entorno UEFI y la Shell


**Pregunta de Razonamiento 1:** Al ejecutar el comando map y dh, vemos protocolos e identificadores en lugar de puertos de hardware fijos. ¿Cuál es la ventaja de seguridad y compatibilidad de este modelo frente al antiguo BIOS?

La ventaja es la abstracción. En BIOS Legacy, el sistema operativo tenía que saber exactamente en qué puerto físico (hardware) estaba conectado el disco para leerlo. UEFI utiliza un modelo de software orientado a objetos: no le importa dónde está conectado físicamente el disco, sino que le asigna un "Handle" (identificador) y carga un "Protocolo" (ej. SIMPLE_FILE_SYSTEM). Esto mejora la compatibilidad, ya que permite cambiar el hardware subyacente (pasar de un disco SATA a un NVMe o un USB) sin que el proceso de arranque deba modificarse, aislando al software de la complejidad del hardware.

![map](./assets/mapQemu.jpeg)

![dh](./assets/dh_5.jpeg)

**Pregunta de Razonamiento 2:** Observando las variables Boot#### y BootOrder, ¿cómo determina el Boot Manager la secuencia de arranque?

La variable BootOrder contiene un arreglo numérico que define la prioridad (por ejemplo: 0001, 0000, 0002). El Boot Manager lee el primer número de BootOrder (ej. 0001), busca la variable asociada Boot0001 para encontrar la ruta del archivo ejecutable EFI (ej. \EFI\BOOT\BOOTX64.EFI) y el dispositivo de almacenamiento al que pertenece. Si ese intento falla, pasa al siguiente número en la lista de BootOrder.


**Pregunta de Razonamiento 3:** En el mapa de memoria (memmap), existen regiones marcadas como RuntimeServicesCode. ¿Por qué estas áreas son un objetivo principal para los desarrolladores de malware (Bootkits)?

En UEFI, la memoria se divide en fases. La mayoría de los servicios de UEFI se descargan de la RAM una vez que el Sistema Operativo toma el control (fase ExitBootServices). Sin embargo, las áreas marcadas como RuntimeServicesCode permanecen activas en la memoria RAM incluso mientras el Sistema Operativo está funcionando, ya que proveen servicios de bajo nivel (como acceder a la NVRAM o reiniciar la PC). Los desarrolladores de malware atacan estas áreas porque les otorga "persistencia": el código malicioso sobrevive a la transición entre la placa madre y el sistema operativo, permitiéndoles ejecutarse con el máximo nivel de privilegios y esquivar los antivirus tradicionales.

---

### Trabajo Práctico 2: Desarrollo, compilación y análisis de seguridad

**Pregunta de Razonamiento 4:** ¿Por qué utilizamos SystemTable->ConOut->OutputString en lugar de la función printf de C?

No utilizamos `printf` porque estamos en una fase previa al Sistema Operativo por lo que no tenemos acceso a las librerias de C donde se define el `printf`.


**Pregunta de Razonamiento 5:** En el pseudocódigo de Ghidra, la condición 0xCC suele aparecer como -52. ¿A qué se debe este fenómeno y por qué importa en ciberseguridad?

¿A qué se debe este fenómeno?
Se debe al sistema de "Complemento a 2" que usan las computadoras para los números negativos y a cómo Ghidra interpreta los datos. El byte hexadecimal `0xCC` equivale a `204` (`11001100` en binario). Sin embargo, si Ghidra asume por defecto que ese valor es `signed char`, el bit inicial en `1` transforma matemáticamente ese `204` en un `-52`. Son exactamente los mismos bits en memoria, solo cambia cómo el descompilador decide mostrarlos.

¿Por qué importa en ciberseguridad?
Análisis de Malware (Anti-Debugging): En ensamblador x86, el byte `0xCC` es la instrucción exacta para pausar el programa. Los creadores de malware lo inyectan para detectar si están siendo analizados y romper los depuradores. Si lees `-52` y no reconoces que es un `0xCC` camuflado, pasarás por alto una trampa.
Explotación de Vulnerabilidades: Confundir si un número es con signo o sin signo causa fallas críticas. Un atacante puede enviar un tamaño de "-52" para pasar una validación de seguridad de tamaño máximo y luego provocar un desbordamiento de memoria.

![GHidra_trabajando](./assets/Ghidra.png)

### Trabajo Práctico 3: Ejecución en Hardware Físico (Bare Metal)

En esta parte del trabajo práctico se siguieron las instrucciones indicadas para preparar un pendrive booteable y ejecutar la aplicación en hardware físico, utilizando una notebook Asus.

Para lograr el arranque desde el pendrive, fue necesario realizar algunos cambios en la configuración de la BIOS, siguiendo las recomendaciones del enunciado del trabajo. Luego de aplicar esos cambios, la notebook pudo bootear correctamente desde el dispositivo USB, sin presentar mayores inconvenientes.

El principal problema apareció al intentar ejecutar la aplicación desarrollada a partir del código proporcionado en el TP2. Al seleccionarla desde la shell UEFI, el sistema quedaba colgado y no permitía continuar con la ejecución ni interactuar normalmente con la shell.

Se realizaron distintas pruebas y modificaciones sobre el código, pero no fue posible lograr que esa versión funcionara correctamente en el hardware físico. Para descartar que el problema estuviera relacionado con la configuración del pendrive, la BIOS o el proceso de booteo, se implementó una aplicación más simple de prueba, consistente en un “Hola mundo”.

Esta segunda aplicación sí pudo ejecutarse correctamente desde la notebook, lo que permitió verificar que el entorno de booteo funcionaba y que el problema estaba específicamente relacionado con la aplicación basada en el código del TP2.


Codigo:
```
#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hola desde UEFI\r\n");

    return EFI_SUCCESS;
}
```

Resultado:

![Bios](./assets/bios.jpg)