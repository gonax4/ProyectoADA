Instrucciones 
Paso 1 — Compilar:
g++ -O2 -std=c++17 -o abc_pfsp abc_pfsp.cpp
Paso 2 — Correrlo con la instancia pequeña y los parámetros por defecto:
./abc_pfsp instancia1_bas1.txt
O indicando los parámetros (lo recomendado para las pruebas del informe):
./abc_pfsp instancia1_bas1.txt 20 30 200 42
Donde cada número significa:
## Parámetros del algoritmo

| Parámetro | Valor de ejemplo | Descripción |
|------------|-----------------|-------------|
| `numAbejas` | 20 | Cantidad de soluciones manejadas simultáneamente por el algoritmo |
| `limite` | 30 | Intentos sin mejora antes de reinicializar una fuente |
| `maxIteraciones` | 200 | Número máximo de iteraciones del algoritmo |
| `semilla` | 42 | Semilla para el generador aleatorio, permite reproducibilidad |

Para las tres instancias:
./abc_pfsp instancia1_bas1.txt  20 30 200 42
./abc_pfsp instancia2_car5.txt  20 30 200 42
./abc_pfsp instancia3_reC01.txt 20 30 200 42

