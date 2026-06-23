# Opción 3: Implementación principal e integración en C++
#### Encargarse de programar el funcionamiento principal del algoritmo ABC en C++.
---
## Descripción de la implementación

Este proyecto implementa el algoritmo Artificial Bee Colony (ABC) para resolver el problema Permutation Flow Shop Scheduling Problem (PFSP). La implementación incluye la lectura de instancias tipo Taillard, el cálculo eficiente del makespan y la ejecución completa de las fases del algoritmo ABC: abejas empleadas, observadoras y exploradoras.

---

## Compilación

Compilar el programa con:

```bash
g++ -O2 -std=c++17 -o abc_pfsp abc_pfsp.cpp
```

---

## Ejecución

Ejecutar utilizando una instancia:

```bash
./abc_pfsp instancia1_bas1.txt
```

O indicando explícitamente los parámetros:

```bash
./abc_pfsp instancia1_bas1.txt 20 30 200 42
```

### Parámetros del algoritmo

| Parámetro      | Valor de ejemplo | Descripción                                                            |
| -------------- | ---------------- | ---------------------------------------------------------------------- |
| numAbejas      | 20               | Cantidad de soluciones manejadas simultáneamente por el algoritmo      |
| limite         | 30               | Número máximo de intentos sin mejora antes de reinicializar una fuente |
| maxIteraciones | 200              | Número total de iteraciones del algoritmo                              |
| semilla        | 42               | Semilla utilizada para el generador aleatorio                          |

### Ejecución de las instancias evaluadas

```bash
./abc_pfsp instancia1_bas1.txt 20 30 200 42
./abc_pfsp instancia2_car5.txt 20 30 200 42
./abc_pfsp instancia3_reC01.txt 20 30 200 42
```
---

## Carga del archivo de entrada

La función `leerInstancia()` abre el archivo .txt especificado por línea de comandos y lee su contenido siguiendo el formato de las instancias de Taillard. La primera línea del archivo contiene la cantidad de trabajos y máquinas. Las líneas siguientes describen los tiempos de procesamiento de cada trabajo en cada máquina mediante pares (índice_máquina, tiempo). Estos datos se almacenan en una estructura Instancia que contiene el número de trabajos, el número de máquinas y la matriz de tiempos de procesamiento.
La función valida que el archivo exista, que los datos estén completos y que los índices de máquina sean coherentes. En caso de error, el programa termina con un mensaje descriptivo. La matriz de tiempos queda indexada como `tiempos[trabajo][maquina]`, donde las filas representan trabajos y las columnas representan máquinas.


---

## Cálculo del makespan

El makespan se calcula mediante la función `calcularMakespanRapido()`, que implementa la fórmula recurrente del PFSP:

```text
C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
```

Esta función es la pieza central del algoritmo ABC, ya que se invoca en cada evaluación de vecino durante las fases de abejas empleadas y observadoras. Para reducir el consumo de memoria, la implementación no construye la matriz completa de tiempos de finalización sino que mantiene únicamente dos vectores de tamaño m (la fila anterior y la fila actual). Esto reduce el uso de memoria de O(n × m) a O(m), lo cual resulta especialmente importante cuando el algoritmo evalúa decenas de miles de soluciones por ejecución.
La función `generarTablaFinalizacion()` sí construye la matriz completa, pero únicamente se llama una vez al final de la ejecución para procesar la mejor solución encontrada y generar los datos de salida para el diagrama de Gantt.


---

## Implementación del algoritmo ABC

Cada solución candidata se representa mediante una estructura FuenteAlimento, que contiene: la permutación de trabajos (orden), el makespan correspondiente y un contador de intentos sin mejora. La función ejecutarABC() orquesta el ciclo completo del algoritmo, cuyas fases se describen de la siguiente manera.
### Inicialización
La función inicializarFuentes() genera numAbejas permutaciones aleatorias de trabajos usando el motor de números pseudoaleatorios mt19937 de la biblioteca estándar de C++. Este generador se inicializa con la semilla indicada por parámetro, lo que garantiza la reproducibilidad de los resultados. A cada fuente se le calcula su makespan desde el inicio para que el algoritmo pueda comparar soluciones de inmediato.
### Fase de abejas empleadas
La función faseEmpleadas() itera sobre cada fuente de alimento y genera un vecino aplicando al azar uno de dos operadores de permutación. El operador de intercambio (swap) selecciona dos posiciones al azar y las intercambia. El operador de inserción extrae un trabajo de una posición al azar y lo inserta en otra posición distinta también al azar. Ambos operadores garantizan que el resultado siga siendo una permutación válida de todos los trabajos.
Tras calcular el makespan del vecino generado, se aplica la selección voraz: si el vecino mejora el makespan actual, reemplaza a la fuente y se reinicia su contador de intentos; de lo contrario, la fuente original se conserva y se incrementa el contador.
### Fase de abejas observadoras
La función faseObservadoras() implementa la selección por ruleta proporcional al fitness. El fitness de cada fuente se calcula como 1 / (1 + makespan), de modo que las fuentes con menor makespan reciben mayor probabilidad de ser seleccionadas. Las observadoras se asignan hasta completar un total de numAbejas asignaciones, concentrando la búsqueda en las regiones más prometedoras del espacio de soluciones. Sobre cada fuente seleccionada se aplica la misma lógica de generación de vecino y selección voraz que en la fase de empleadas.
### Fase de abejas exploradoras
La función faseExploradoras() recorre todas las fuentes y verifica si alguna ha superado el parámetro límite de intentos consecutivos sin mejora. En ese caso, la fuente se considera agotada y se reemplaza por una nueva permutación generada completamente al azar, simulando el comportamiento de una abeja exploradora que abandona una zona improductiva y explora una nueva región del espacio de búsqueda. Se mantiene un contador de reinicios para reportar esta información al finalizar la ejecución.
### Actualización de la mejor solución global
Al final de cada iteración, el ciclo revisa todas las fuentes y actualiza la mejor solución global si alguna ha obtenido un makespan menor al registrado. El mejor orden y el mejor makespan global nunca se sobreescriben por soluciones peores, garantizando que el resultado final sea el óptimo encontrado durante toda la ejecución.


---

## Funciones principales
La tabla siguiente resume las funciones más relevantes del programa, con su propósito y la fase del algoritmo a la que pertenecen.

## Funciones principales

| Función | Descripción | Fase ABC |
|----------|------------|----------|
| `inicializarFuentes()` | Genera la población inicial de soluciones aleatorias. Cada fuente de alimento es una permutación de trabajos con su makespan ya calculado. | Inicialización |
| `faseEmpleadas()` | Cada abeja empleada genera un vecino de su fuente usando swap o inserción y aplica selección voraz. | Empleadas |
| `faseObservadoras()` | Selecciona fuentes por ruleta proporcional al fitness. Las mejores fuentes reciben más atención de búsqueda. | Observadoras |
| `faseExploradoras()` | Abandona las fuentes agotadas (que superaron el límite de intentos) y las reemplaza con soluciones aleatorias nuevas. | Exploradoras |
| `generarVecino()` | Aplica al azar un operador de vecindad: intercambio de dos posiciones (swap) o extracción e inserción de un trabajo. | Empleadas / Observadoras |
| `calcularMakespanRapido()` | Versión optimizada del cálculo de makespan que usa solo dos filas en lugar de la matriz completa para reducir el uso de memoria. | Evaluación |
| `ejecutarABC()` | Orquesta el ciclo completo del algoritmo: inicialización, iteraciones, actualización de la mejor solución y registro del historial. | Ciclo principal |
| `exportarConvergenciaCSV()` | Guarda el historial del mejor makespan por iteración en un archivo CSV para construir la curva de convergencia. | Salida |
| `exportarDatosGanttCSV()` | Exporta los tiempos de inicio, fin y duración de cada operación de la mejor solución, listos para el diagrama de Gantt. | Salida |

---

## Resultados generados
Al finalizar su ejecución, el programa muestra en consola la mejor secuencia de trabajos encontrada, el mejor makespan, el número de iteraciones ejecutadas, el número de reinicios realizados por la fase exploradora, el tiempo de ejecución en segundos y los parámetros utilizados.
Además, genera cuatro archivos de salida cuyo nombre incorpora el nombre de la instancia procesada:


| Archivo | Descripción |
|----------|------------|
| `salida_abc_<instancia>_resumen.txt` | Resumen completo de la ejecución con parámetros y resultados. |
| `salida_abc_<instancia>_tabla_finalizacion.csv` | Tabla de tiempos de finalización de la mejor solución encontrada. |
| `salida_abc_<instancia>_datos_gantt.csv` | Datos necesarios para construir el diagrama de Gantt. |
| `salida_abc_<instancia>_convergencia.csv` | Historial del mejor makespan por iteración para la curva de convergencia. |
---


