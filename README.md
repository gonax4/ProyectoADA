# Proyecto ADA – PFSP con Artificial Bee Colony (ABC) · Grupo 6

---

## Descripción del proyecto

Este repositorio contiene la implementación del algoritmo metaheurístico **Artificial Bee Colony (ABC)** para resolver el **Permutation Flow Shop Scheduling Problem (PFSP)**, desarrollado para el curso de Análisis y Diseño de Algoritmos de la Universidad de Lima (Evaluación 3 – G6).

El PFSP consiste en determinar el orden de procesamiento de un conjunto de **n trabajos** que pasan secuencialmente por **m máquinas**, de forma que se minimice el **makespan** (tiempo total de finalización). Una solución se representa como una permutación de trabajos, por ejemplo `[J3, J1, J4, J2, J5]`, que se aplica de forma idéntica en todas las máquinas.

---

## Archivos del proyecto

| Archivo | Descripción |
|---------|-------------|
| `abc_pfsp.cpp` | Implementación completa del algoritmo ABC adaptado al PFSP en C++17. |
| `instancia1_bas1.txt` | Instancia pequeña: 5 trabajos × 4 máquinas. |
| `instancia2_car5.txt` | Instancia mediana: 10 trabajos × 6 máquinas. |
| `instancia3_reC01.txt` | Instancia grande: 20 trabajos × 5 máquinas. |
| `README.md` | Este archivo. |

---

## Compilación

```bash
g++ -O2 -std=c++17 -o abc_pfsp abc_pfsp.cpp
```

Requiere un compilador compatible con C++17 (GCC 7+, Clang 5+).

---

## Modos de ejecución

### Ejecución estándar (una corrida)

```bash
./abc_pfsp <archivo_instancia.txt> [numAbejas] [limite] [maxIteraciones] [semilla]
```

**Ejemplos:**

```bash
# Con parámetros por defecto (numAbejas=20, limite=30, maxIteraciones=200, semilla=aleatoria)
./abc_pfsp instancia1_bas1.txt

# Con parámetros explícitos y semilla fija (reproducible)
./abc_pfsp instancia1_bas1.txt 20 30 200 42
./abc_pfsp instancia2_car5.txt 20 30 200 42
./abc_pfsp instancia3_reC01.txt 20 30 200 42
```

### Modo experimento (10 ejecuciones automáticas con tabla de estadísticas)

Realiza 10 corridas con semillas predefinidas y genera automáticamente la tabla resumen requerida por la rúbrica (mejor makespan, peor makespan, promedio, desviación estándar, tiempo promedio, mejor secuencia), la comparación contra la heurística base NEH, y todos los archivos de salida de la mejor corrida.

```bash
./abc_pfsp --experimento <archivo_instancia.txt> [numAbejas] [limite] [maxIteraciones]
```

**Ejemplos (recomendados para reproducir los resultados del informe):**

```bash
./abc_pfsp --experimento instancia1_bas1.txt 20 30 200
./abc_pfsp --experimento instancia2_car5.txt 20 30 200
./abc_pfsp --experimento instancia3_reC01.txt 20 30 200
```

---

## Parámetros del algoritmo

| Parámetro | Valor por defecto | Descripción |
|-----------|-------------------|-------------|
| `numAbejas` | 20 | Número de fuentes de alimento (= abejas empleadas). También determina las abejas observadoras. |
| `limite` | 30 | Máximo de intentos sin mejora que tolera una fuente antes de ser abandonada por una exploradora. |
| `maxIteraciones` | 200 | Número total de ciclos del algoritmo (criterio de parada). |
| `semilla` | aleatoria | Semilla para el generador `mt19937`. Fijarla garantiza reproducibilidad exacta. |

---

## Descripción del algoritmo ABC adaptado al PFSP

### Representación de soluciones

Cada **fuente de alimento** es una permutación de trabajos (índices 0 a n-1). El fitness se mide con el makespan: a menor makespan, mejor fuente.

### Fases del ciclo ABC

| Fase | Función en el código | Descripción |
|------|----------------------|-------------|
| Inicialización | `inicializarFuentes()` | Genera `numAbejas` permutaciones aleatorias usando `mt19937` con la semilla indicada. |
| Abejas empleadas | `faseEmpleadas()` | Cada abeja genera un vecino de su fuente mediante **swap** o **inserción** al azar. Aplica selección voraz: acepta el vecino solo si mejora el makespan. |
| Abejas observadoras | `faseObservadoras()` | Selecciona fuentes por **ruleta proporcional al fitness** (`1/(1+makespan)`). Las fuentes con menor makespan atraen más observadoras. Aplica la misma búsqueda local voraz. |
| Abejas exploradoras | `faseExploradoras()` | Si una fuente supera el `limite` de intentos consecutivos sin mejora, se reemplaza por una permutación completamente nueva al azar. |
| Actualización global | Ciclo principal en `ejecutarABC()` | Al final de cada iteración se actualiza la mejor solución global sin sobreescribirla si empeora. |

### Operadores de vecindad

- **Swap:** intercambia dos posiciones seleccionadas al azar.
- **Inserción:** extrae un trabajo de una posición y lo inserta en otra posición distinta.

### Cálculo del makespan

Se utiliza la recurrencia del PFSP:

```
C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
```

La función `calcularMakespanRapido()` mantiene solo dos vectores de tamaño m (fila anterior y fila actual), reduciendo la memoria de O(n×m) a O(m). Esta versión optimizada se usa en cada evaluación durante las fases del algoritmo.

La función `generarTablaFinalizacion()` construye la matriz completa, pero solo se llama una vez al final para generar los datos de salida (Gantt y tabla de finalización).

### Detección automática de formato de instancia

La función `leerInstancia()` detecta automáticamente si el archivo usa:

- **Formato A (matriz simple):** exactamente `n × m` números, una fila por trabajo con sus tiempos en orden M1..Mm.
- **Formato B (pares máquina-tiempo):** exactamente `2 × n × m` números, pares `(índice_máquina, tiempo)` por trabajo. También detecta si la numeración de máquinas es base 0 (0..m-1) o base 1 (1..m) y ajusta automáticamente.

Si el archivo no coincide con ninguno de los dos formatos, el programa informa el error con el detalle exacto del número de valores esperados vs. encontrados.

### Heurística base de comparación (NEH)

El programa incluye la heurística **NEH** (Nawaz, Enscore y Ham, 1983), que es el método constructivo de referencia para el PFSP. Se calcula automáticamente antes de correr ABC y se reporta junto con el makespan ABC para cuantificar la mejora. NEH fue elegido como baseline porque produce resultados de buena calidad con costo computacional mínimo, lo que hace la comparación más exigente y creíble que una secuencia aleatoria.

---

## Archivos de salida generados

### Modo estándar (una corrida)

| Archivo | Descripción |
|---------|-------------|
| `salida_abc_<instancia>_resumen.txt` | Parámetros, mejor secuencia, makespan, iteraciones, reinicios, tiempo de ejecución y comparación con NEH. |
| `salida_abc_<instancia>_tabla_finalizacion.csv` | Tabla C[i][j] de tiempos de finalización de la mejor solución. |
| `salida_abc_<instancia>_datos_gantt.csv` | Inicio, fin y duración de cada operación (trabajo × máquina). |
| `salida_abc_<instancia>_convergencia.csv` | Mejor makespan por iteración (para graficar la curva de convergencia). |
| `grafica_convergencia_<instancia>.svg` | Curva de convergencia generada automáticamente (eje X: iteraciones, eje Y: mejor makespan). |
| `grafica_gantt_<instancia>.svg` | Diagrama de Gantt de la mejor solución con colores por trabajo, tiempos de inicio/fin y makespan final. |

### Modo experimento (10 corridas)

Todos los archivos anteriores (prefijo `salida_experimento_` y `grafica_*_experimento_`) referidos a la **mejor corrida** del experimento, más:

| Archivo | Descripción |
|---------|-------------|
| `salida_experimento_<instancia>_ejecuciones.csv` | Detalle fila a fila de las 10 ejecuciones (semilla, makespan, tiempo, iteraciones, secuencia). |
| `salida_experimento_<instancia>_resumen.txt` | Tabla resumen estadística + comparación con NEH, lista para copiar al informe. |

---

## Estructura del código fuente

```
abc_pfsp.cpp
├── Estructuras de datos
│   ├── Instancia           — trabajos, maquinas, tiempos, formatoDetectado
│   ├── FuenteAlimento      — orden (permutación), makespan, intentos
│   ├── ParametrosABC       — numAbejas, limite, maxIteraciones, semilla
│   ├── ResultadoABC        — mejorOrden, mejorMakespan, historial, tiempos...
│   └── EjecucionExperimento — datos de una corrida dentro del modo experimento
│
├── Lectura y validación
│   ├── leerInstancia()     — parsea el archivo con detección automática de formato
│   └── validarOrden()      — verifica que la permutación sea válida
│
├── Cálculo del makespan
│   ├── calcularMakespanRapido()    — versión O(m) usada en evaluaciones internas
│   └── generarTablaFinalizacion()  — versión completa O(n×m) para exportar
│
├── Heurística base
│   └── heuristicaNEH()     — NEH clásico: suma de tiempos + inserción voraz
│
├── Operadores de vecindad
│   ├── operadorSwap()       — intercambia dos posiciones
│   ├── operadorInsercion()  — extrae e inserta un trabajo
│   └── generarVecino()      — elige al azar entre swap e inserción
│
├── Fases del algoritmo ABC
│   ├── inicializarFuentes()
│   ├── faseEmpleadas()
│   ├── faseObservadoras()
│   ├── faseExploradoras()
│   └── ejecutarABC()        — ciclo principal, retorna ResultadoABC
│
├── Salida en consola
│   ├── mostrarOrden()
│   ├── mostrarMatriz()
│   └── mostrarTablaFinalizacion()  — muestra C[i][j] directamente en terminal
│
├── Exportación a archivo
│   ├── exportarTablaFinalizacionCSV()
│   ├── exportarDatosGanttCSV()
│   ├── exportarConvergenciaCSV()
│   ├── exportarResumenABC_TXT()         — incluye comparación con NEH
│   ├── exportarExperimentoCSV()         — detalle de las 10 corridas
│   └── exportarResumenExperimentoTXT()  — tabla estadística del experimento
│
└── Visualizaciones SVG (generadas automáticamente)
    ├── generarGraficaConvergenciaSVG()  — curva de convergencia
    └── generarGraficaGanttSVG()         — diagrama de Gantt con colores
```

---

## Notas de reproducibilidad

- Fijar la misma `semilla` garantiza resultados idénticos en cualquier plataforma con C++17.
- El modo `--experimento` usa las semillas fijas `{42, 123, 456, 789, 1001, 2024, 3141, 9999, 11, 777}`.
- Los archivos SVG son compatibles con cualquier navegador moderno.
- El formato de instancia se detecta automáticamente, por lo que no es necesario modificar el código para cambiar entre los archivos de ejemplo.
