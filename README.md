 
# Proyecto ADA - PFSP con Artificial Bee Colony - G6

Este repositorio contiene la base inicial del proyecto de Análisis y Diseño de Algoritmos para resolver el **Permutation Flow Shop Scheduling Problem (PFSP)** mediante el algoritmo **Artificial Bee Colony (ABC)**.

## Contenido actual

Esta primera versión corresponde a la base del PFSP y cálculo del makespan. El programa permite:

* Leer instancias `.txt`.
* Identificar la cantidad de trabajos y máquinas.
* Guardar los tiempos de procesamiento en una matriz.
* Evaluar un orden de trabajos.
* Validar que el orden ingresado sea una permutación válida.
* Calcular la tabla de tiempos de finalización.
* Obtener el makespan.
* Generar datos de inicio y fin para elaborar el diagrama de Gantt.
* Exportar archivos `.csv` y `.txt` con los resultados.

## Archivos principales

* `main.cpp`: código base del PFSP.
* `instancia1_bas1.txt`: instancia pequeña de 5 trabajos y 4 máquinas.
* `instancia2_car5.txt`: instancia mediana de 10 trabajos y 6 máquinas.
* `instancia3_reC01.txt`: instancia grande de 20 trabajos y 5 máquinas.
* `readme_instancias.txt`: explicación del formato de las instancias.

## Compilación

Para compilar el programa, ejecutar:

```bash
g++ main.cpp -o pfsp
```

## Ejecución con orden natural

Si se ejecuta solo con el archivo de instancia, el programa evalúa el orden natural de trabajos:

```bash
./pfsp instancia1_bas1.txt
```

Esto evalúa:

```txt
[J1, J2, J3, J4, J5]
```

## Ejecución con orden manual

También se puede ingresar un orden específico de trabajos. Por ejemplo:

```bash
./pfsp instancia1_bas1.txt 3 1 4 2 5
```

Esto evalúa la secuencia:

```txt
[J3, J1, J4, J2, J5]
```

El programa recibe los trabajos desde 1 para facilitar la lectura, pero internamente usa índices desde 0.

## Archivos generados

Después de ejecutar el programa, se generan archivos como:

```txt
salida_instancia1_bas1_tabla_finalizacion.csv
salida_instancia1_bas1_datos_gantt.csv
salida_instancia1_bas1_resumen.txt
```

Estos archivos contienen:

* La tabla de tiempos de finalización.
* Los datos de inicio y fin para el diagrama de Gantt.
* Un resumen de la instancia evaluada, el orden usado y el makespan obtenido.

## Uso para la integración con ABC

La parte del algoritmo ABC puede reutilizar directamente las siguientes funciones:

```cpp
leerInstancia()
validarOrden()
generarTablaFinalizacion()
calcularMakespan()
```

La función más importante para la integración es:

```cpp
long long makespan = calcularMakespan(instancia, ordenTrabajos);
```

El algoritmo ABC deberá generar diferentes permutaciones de trabajos y usar esta función para evaluar la calidad de cada solución. Mientras menor sea el makespan, mejor será la solución.

## Nota importante

Los resultados actuales no representan la solución óptima final. Solo validan que el módulo base puede leer las instancias y calcular correctamente el makespan de cualquier secuencia de trabajos. La búsqueda de mejores secuencias será realizada posteriormente mediante el algoritmo Artificial Bee Colony.
