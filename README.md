# Artificial Bee Colony para Permutation Flow Shop Scheduling Problem (PFSP)

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

## Estructura general del programa

La implementación se divide en tres componentes principales:

1. Lectura y validación de instancias.
2. Evaluación de soluciones mediante el cálculo del makespan.
3. Ejecución del algoritmo Artificial Bee Colony.

---

## Carga del archivo de entrada

La función `leerInstancia()` abre el archivo de instancia especificado por línea de comandos y procesa su contenido siguiendo el formato de Taillard.

La primera línea contiene el número de trabajos y máquinas. Las líneas siguientes contienen pares `(máquina, tiempo)` que representan los tiempos de procesamiento de cada trabajo en cada máquina.

Los datos son almacenados en una estructura denominada `Instancia`, que contiene:

* Número de trabajos.
* Número de máquinas.
* Matriz de tiempos de procesamiento.

La función valida:

* Existencia del archivo.
* Integridad de los datos.
* Coherencia de los índices de máquina.

---

## Cálculo del makespan

La función `calcularMakespanRapido()` implementa la recurrencia clásica del PFSP:

```text
C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
```

Para reducir el consumo de memoria, únicamente se mantienen dos vectores de tamaño `m`, reduciendo la complejidad espacial de:

```text
O(n × m)
```

a:

```text
O(m)
```

La función `generarTablaFinalizacion()` construye la matriz completa únicamente para la mejor solución encontrada y se utiliza para generar los datos necesarios para la visualización final.

---

## Implementación del algoritmo ABC

Cada solución candidata se representa mediante una estructura `FuenteAlimento`, compuesta por:

* Permutación de trabajos.
* Makespan asociado.
* Contador de intentos sin mejora.

La función principal `ejecutarABC()` coordina todas las fases del algoritmo.

### Inicialización

La función `inicializarFuentes()` genera una población inicial de soluciones aleatorias utilizando el generador pseudoaleatorio `mt19937`.

Cada solución recibe inmediatamente una evaluación de makespan para poder participar en el proceso de optimización.

### Fase de abejas empleadas

La función `faseEmpleadas()` genera vecinos utilizando dos operadores:

* Intercambio (Swap).
* Inserción (Insertion).

Luego aplica selección voraz:

* Si el vecino mejora la solución actual, se reemplaza.
* En caso contrario, se conserva la solución original.

### Fase de abejas observadoras

La función `faseObservadoras()` utiliza selección por ruleta basada en fitness.

El fitness se calcula mediante:

```text
fitness = 1 / (1 + makespan)
```

Las soluciones con menor makespan tienen mayor probabilidad de ser seleccionadas para continuar la exploración.

### Fase de abejas exploradoras

La función `faseExploradoras()` identifica fuentes que han superado el límite de intentos sin mejora.

Estas soluciones son descartadas y reemplazadas por nuevas permutaciones aleatorias para fomentar la exploración del espacio de búsqueda.

### Actualización de la mejor solución global

Al finalizar cada iteración, se revisan todas las fuentes de alimento.

Si alguna solución presenta un makespan inferior al mejor registrado, se actualiza la mejor solución global.

---

## Funciones principales

| Función                   | Descripción                                                    | Fase ABC                 |
| ------------------------- | -------------------------------------------------------------- | ------------------------ |
| inicializarFuentes()      | Genera la población inicial de soluciones aleatorias.          | Inicialización           |
| faseEmpleadas()           | Genera vecinos y aplica selección voraz.                       | Empleadas                |
| faseObservadoras()        | Selecciona soluciones mediante ruleta proporcional al fitness. | Observadoras             |
| faseExploradoras()        | Reinicializa soluciones estancadas.                            | Exploradoras             |
| generarVecino()           | Aplica operadores swap e inserción.                            | Empleadas / Observadoras |
| calcularMakespanRapido()  | Calcula eficientemente el makespan.                            | Evaluación               |
| ejecutarABC()             | Coordina el ciclo completo del algoritmo.                      | Ciclo principal          |
| exportarConvergenciaCSV() | Exporta la curva de convergencia.                              | Salida                   |
| exportarDatosGanttCSV()   | Exporta los datos para el diagrama de Gantt.                   | Salida                   |

---

## Resultados generados

Al finalizar la ejecución, el programa muestra:

* Mejor secuencia encontrada.
* Mejor makespan obtenido.
* Número de iteraciones ejecutadas.
* Número de reinicios realizados.
* Tiempo total de ejecución.
* Parámetros utilizados.

### Archivos generados

| Archivo                                       | Descripción                                |
| --------------------------------------------- | ------------------------------------------ |
| salida_abc_<instancia>_resumen.txt            | Resumen completo de la ejecución           |
| salida_abc_<instancia>_tabla_finalizacion.csv | Tabla de tiempos de finalización           |
| salida_abc_<instancia>_datos_gantt.csv        | Datos para construir el diagrama de Gantt  |
| salida_abc_<instancia>_convergencia.csv       | Historial del mejor makespan por iteración |

---

## Referencias

* Taillard, É. (1993). Benchmarks for basic scheduling problems.
* Karaboga, D. (2005). An Idea Based on Honey Bee Swarm for Numerical Optimization.

