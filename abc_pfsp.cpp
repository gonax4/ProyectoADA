#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <random>
#include <chrono>
#include <climits>
#include <cmath>

using namespace std;
using namespace std::chrono;

/*
    =========================================================================
    Opcion 3: Implementacion principal e integracion en C++
    Algoritmo: Colonia de Abejas Artificiales (Artificial Bee Colony - ABC)
    Problema: Permutation Flow Shop Scheduling Problem (PFSP)
    Grupo 6
    =========================================================================

    Este archivo integra:
      - La lectura de instancias (con deteccion automatica de formato),
        la matriz de tiempos, la tabla de finalizacion y el calculo del
        makespan (Opcion 1).
      - El ciclo completo del algoritmo ABC adaptado al PFSP (Opcion 2):
          * Generacion de soluciones iniciales (fuentes de alimento)
          * Fase de abejas empleadas
          * Fase de abejas observadoras
          * Fase de abejas exploradoras
          * Actualizacion de la mejor solucion
      - Una heuristica base de comparacion (NEH), para poder reportar
        cuanto mejora el ABC frente a un metodo constructivo clasico
        (pedido en la presentacion oral del proyecto).
      - Registro de la evolucion del mejor makespan por iteracion
        (curva de convergencia), tanto en CSV como en grafica SVG.
      - Generacion de los datos y la grafica del diagrama de Gantt de la
        mejor solucion encontrada.
      - Modo --experimento: corre 10 ejecuciones con semillas fijas,
        calcula mejor/peor/promedio/desviacion/tiempo promedio, y GUARDA
        todo el detalle (no solo lo imprime en consola): una fila por
        ejecucion en CSV, un resumen en TXT, y la curva de convergencia +
        diagrama de Gantt de la mejor corrida encontrada.

    Formato de archivo de instancia (autodetectado, no es necesario indicarlo):
        Primera linea: <trabajos> <maquinas>
        Luego, una de estas dos variantes (se distingue por la cantidad
        de numeros restantes en el archivo):
          A) Matriz simple: una fila por trabajo con sus "maquinas" tiempos
             de procesamiento en orden M1..Mm (igual a la tabla de ejemplo
             del enunciado).
          B) Pares (maquina, tiempo): una fila por trabajo con pares de
             numeros. Se detecta solo si la numeracion de maquinas viene
             en base 0 o en base 1.

    Representacion de una solucion:
        Una fuente de alimento = una permutacion de trabajos,
        por ejemplo [J3, J1, J4, J2, J5] -> internamente {2,0,3,1,4}.
        La calidad (fitness) de la fuente se mide con su makespan:
        a menor makespan, mejor fuente.

    Operadores de vecindad usados para generar nuevas soluciones:
        - Intercambio (swap): se intercambian dos posiciones al azar.
        - Insercion (insertion): se extrae un trabajo de una posicion
          y se inserta en otra posicion al azar.

    Uso:
        ./abc_pfsp archivo_instancia.txt [numAbejas] [limite] [maxIteraciones] [semilla]
        ./abc_pfsp --experimento archivo_instancia.txt [numAbejas] [limite] [maxIteraciones]

    Ejemplo:
        ./abc_pfsp instancia1_bas1.txt 20 30 200 42
        ./abc_pfsp --experimento instancia1_bas1.txt 20 30 200

    Si no se indican los parametros opcionales, se usan valores por defecto.
*/

// ----------------------------------------------------------------------
// PARTE 1 (Opcion 1): lectura de instancias, makespan, tabla, Gantt
// ----------------------------------------------------------------------

struct Instancia {
    int trabajos;
    int maquinas;
    vector<vector<int>> tiempos;
    string formatoDetectado;  // descripcion del formato de archivo detectado (informativo)
};

string obtenerNombreBase(const string& ruta) {
    size_t inicio = ruta.find_last_of("/\\");
    string nombre = (inicio == string::npos) ? ruta : ruta.substr(inicio + 1);

    size_t punto = nombre.find_last_of(".");
    if (punto != string::npos) {
        nombre = nombre.substr(0, punto);
    }

    return nombre;
}

// Lee una instancia PFSP detectando automaticamente el formato del archivo.
//
// El enunciado solo exige "cargar una matriz de tiempos de procesamiento
// desde el archivo .txt especificado", sin fijar un formato unico, y las
// instancias reales (instancia1_bas1.txt, instancia2_car5.txt,
// instancia3_reC01.txt) pueden venir en mas de una convencion habitual.
// Por eso esta funcion soporta dos formatos, eligiendo segun la cantidad
// de numeros que quedan despues de la primera linea (trabajos, maquinas):
//
//   FORMATO A - "matriz simple": exactamente (trabajos x maquinas) numeros,
//       una fila por trabajo con sus tiempos en orden M1..Mm (tal como se
//       muestra en la tabla de ejemplo del enunciado).
//
//   FORMATO B - "pares maquina-tiempo": exactamente (2 x trabajos x maquinas)
//       numeros, una fila por trabajo con pares (maquina, tiempo). En este
//       formato tambien se detecta si la numeracion de maquinas viene en
//       base 1 (1..m) en vez de base 0 (0..m-1), y se ajusta sola.
//
// Si la cantidad de numeros no calza con ninguno de los dos formatos, se
// informa el error con el detalle exacto para poder corregirlo rapido.
Instancia leerInstancia(const string& nombreArchivo) {
    ifstream archivo(nombreArchivo);

    if (!archivo.is_open()) {
        cerr << "Error: no se pudo abrir el archivo " << nombreArchivo << endl;
        exit(1);
    }

    Instancia instancia;

    if (!(archivo >> instancia.trabajos >> instancia.maquinas)) {
        cerr << "Error: el archivo no tiene una primera linea valida con trabajos y maquinas." << endl;
        exit(1);
    }

    if (instancia.trabajos <= 0 || instancia.maquinas <= 0) {
        cerr << "Error: la cantidad de trabajos y maquinas debe ser mayor que cero." << endl;
        exit(1);
    }

    int n = instancia.trabajos;
    int m = instancia.maquinas;

    // Se leen TODOS los numeros restantes del archivo antes de decidir
    // como interpretarlos, para poder distinguir entre los dos formatos.
    vector<long long> restantes;
    long long valor;
    while (archivo >> valor) {
        restantes.push_back(valor);
    }
    archivo.close();

    size_t esperadoMatriz = (size_t)n * (size_t)m;
    size_t esperadoPares   = 2 * esperadoMatriz;

    instancia.tiempos.assign(n, vector<int>(m, 0));

    if (restantes.size() == esperadoMatriz) {
        // ---- FORMATO A: matriz simple (job-major, sin indice de maquina) ----
        instancia.formatoDetectado = "matriz simple (sin indice de maquina)";

        size_t idx = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                long long tiempo = restantes[idx++];
                if (tiempo < 0) {
                    cerr << "Error: tiempo de procesamiento negativo en J" << i + 1
                         << ", M" << j + 1 << "." << endl;
                    exit(1);
                }
                instancia.tiempos[i][j] = (int)tiempo;
            }
        }

    } else if (restantes.size() == esperadoPares) {
        // ---- FORMATO B: pares (maquina, tiempo) por trabajo ----
        // Primero se detecta si la numeracion de maquinas es base 0 o base 1
        // revisando el rango de todos los indices de maquina leidos.
        long long minMaquina = LLONG_MAX, maxMaquina = LLONG_MIN;
        for (size_t idx = 0; idx < restantes.size(); idx += 2) {
            long long maquina = restantes[idx];
            minMaquina = min(minMaquina, maquina);
            maxMaquina = max(maxMaquina, maquina);
        }

        int desplazamiento;
        if (minMaquina == 0 && maxMaquina == m - 1) {
            desplazamiento = 0;
            instancia.formatoDetectado = "pares (maquina, tiempo), maquinas base 0";
        } else if (minMaquina == 1 && maxMaquina == m) {
            desplazamiento = -1;
            instancia.formatoDetectado = "pares (maquina, tiempo), maquinas base 1 (ajustado a base 0)";
        } else {
            cerr << "Error: los indices de maquina en el archivo van de " << minMaquina
                 << " a " << maxMaquina << ", lo cual no corresponde a una numeracion"
                 << " base 0 (0.." << (m - 1) << ") ni base 1 (1.." << m << ")." << endl;
            exit(1);
        }

        size_t idx = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                long long maquina = restantes[idx++] + desplazamiento;
                long long tiempo  = restantes[idx++];

                if (maquina < 0 || maquina >= m) {
                    cerr << "Error: indice de maquina invalido en J" << i + 1 << ": " << maquina << endl;
                    exit(1);
                }
                if (tiempo < 0) {
                    cerr << "Error: tiempo de procesamiento negativo en J" << i + 1 << "." << endl;
                    exit(1);
                }

                instancia.tiempos[i][(int)maquina] = (int)tiempo;
            }
        }

    } else {
        cerr << "Error: el archivo " << nombreArchivo << " tiene " << restantes.size()
             << " numeros despues del encabezado (trabajos=" << n << ", maquinas=" << m << ")." << endl;
        cerr << "  Se esperaba " << esperadoMatriz << " numeros (formato matriz simple)" << endl;
        cerr << "  o " << esperadoPares << " numeros (formato pares maquina-tiempo)." << endl;
        cerr << "  Revisa el formato real del archivo de instancia." << endl;
        exit(1);
    }

    return instancia;
}

bool validarOrden(const vector<int>& ordenTrabajos, int cantidadTrabajos) {
    if ((int)ordenTrabajos.size() != cantidadTrabajos) {
        return false;
    }

    vector<bool> visto(cantidadTrabajos, false);

    for (int trabajo : ordenTrabajos) {
        if (trabajo < 0 || trabajo >= cantidadTrabajos) {
            return false;
        }
        if (visto[trabajo]) {
            return false;
        }
        visto[trabajo] = true;
    }

    return true;
}

vector<int> generarOrdenInicial(int cantidadTrabajos) {
    vector<int> orden;
    for (int i = 0; i < cantidadTrabajos; i++) {
        orden.push_back(i);
    }
    return orden;
}

// C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
vector<vector<long long>> generarTablaFinalizacion(
    const Instancia& instancia,
    const vector<int>& ordenTrabajos
) {
    int n = ordenTrabajos.size();
    int m = instancia.maquinas;

    vector<vector<long long>> C(n, vector<long long>(m, 0));

    for (int i = 0; i < n; i++) {
        int trabajoActual = ordenTrabajos[i];

        for (int j = 0; j < m; j++) {
            int tiempoProcesamiento = instancia.tiempos[trabajoActual][j];

            long long finTrabajoAnterior = (i > 0) ? C[i - 1][j] : 0;
            long long finMaquinaAnterior = (j > 0) ? C[i][j - 1] : 0;

            C[i][j] = max(finTrabajoAnterior, finMaquinaAnterior) + tiempoProcesamiento;
        }
    }

    return C;
}

// Calcula el makespan de un orden sin construir toda la matriz por completo
// en memoria adicional: usa solo el vector de la "fila" anterior.
// Esta version optimizada es la que usa el algoritmo ABC en cada evaluacion,
// ya que se llama miles de veces.
long long calcularMakespanRapido(
    const Instancia& instancia,
    const vector<int>& ordenTrabajos
) {
    int m = instancia.maquinas;
    vector<long long> filaAnterior(m, 0);
    vector<long long> filaActual(m, 0);

    for (size_t i = 0; i < ordenTrabajos.size(); i++) {
        int trabajoActual = ordenTrabajos[i];

        for (int j = 0; j < m; j++) {
            int tiempoProcesamiento = instancia.tiempos[trabajoActual][j];

            long long finTrabajoAnterior = (i > 0) ? filaAnterior[j] : 0;
            long long finMaquinaAnterior = (j > 0) ? filaActual[j - 1] : 0;

            filaActual[j] = max(finTrabajoAnterior, finMaquinaAnterior) + tiempoProcesamiento;
        }

        filaAnterior = filaActual;
    }

    return filaAnterior.back();
}

long long calcularMakespan(
    const Instancia& instancia,
    const vector<int>& ordenTrabajos
) {
    return calcularMakespanRapido(instancia, ordenTrabajos);
}

void mostrarOrden(const vector<int>& ordenTrabajos) {
    cout << "[";
    for (size_t i = 0; i < ordenTrabajos.size(); i++) {
        cout << "J" << ordenTrabajos[i] + 1;
        if (i < ordenTrabajos.size() - 1) cout << ", ";
    }
    cout << "]";
}

string ordenAString(const vector<int>& ordenTrabajos) {
    ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < ordenTrabajos.size(); i++) {
        oss << "J" << ordenTrabajos[i] + 1;
        if (i < ordenTrabajos.size() - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

void exportarTablaFinalizacionCSV(
    const string& nombreArchivoSalida,
    const vector<vector<long long>>& C,
    const vector<int>& ordenTrabajos
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Trabajo";
    if (!C.empty()) {
        for (int j = 0; j < (int)C[0].size(); j++) salida << ",M" << j + 1;
    }
    salida << "\n";

    for (int i = 0; i < (int)C.size(); i++) {
        salida << "J" << ordenTrabajos[i] + 1;
        for (int j = 0; j < (int)C[i].size(); j++) salida << "," << C[i][j];
        salida << "\n";
    }

    salida.close();
}

void exportarDatosGanttCSV(
    const string& nombreArchivoSalida,
    const Instancia& instancia,
    const vector<int>& ordenTrabajos,
    const vector<vector<long long>>& C
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Posicion,Trabajo,Maquina,Inicio,Fin,Duracion\n";

    for (int i = 0; i < (int)ordenTrabajos.size(); i++) {
        int trabajoActual = ordenTrabajos[i];

        for (int j = 0; j < instancia.maquinas; j++) {
            long long fin = C[i][j];
            int duracion = instancia.tiempos[trabajoActual][j];
            long long inicio = fin - duracion;

            salida << i + 1 << ","
                   << "J" << trabajoActual + 1 << ","
                   << "M" << j + 1 << ","
                   << inicio << ","
                   << fin << ","
                   << duracion << "\n";
        }
    }

    salida.close();
}

// Exporta la evolucion del mejor makespan por iteracion (curva de convergencia).
void exportarConvergenciaCSV(
    const string& nombreArchivoSalida,
    const vector<long long>& historialMejorMakespan
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Iteracion,MejorMakespan\n";
    for (size_t i = 0; i < historialMejorMakespan.size(); i++) {
        salida << i + 1 << "," << historialMejorMakespan[i] << "\n";
    }

    salida.close();
}

// Genera la grafica SVG de la curva de convergencia (mejor makespan vs.
// iteracion). Se extrajo a una funcion independiente (antes era codigo
// suelto dentro de main) para que tambien se pueda usar desde el modo
// --experimento, sobre la mejor corrida encontrada.
void generarGraficaConvergenciaSVG(
    const string& archivoSVG,
    const string& etiquetaInstancia,
    const vector<long long>& historial
) {
    ofstream svg(archivoSVG);
    if (!svg.is_open()) {
        cerr << "Advertencia: no se pudo crear " << archivoSVG << endl;
        return;
    }

    const int W = 800, H = 300;
    const int padL = 70, padR = 30, padT = 40, padB = 50;
    int iW = W - padL - padR;
    int iH = H - padT - padB;

    long long minV = *min_element(historial.begin(), historial.end());
    long long maxV = *max_element(historial.begin(), historial.end());
    if (maxV == minV) maxV = minV + 1;

    auto toX = [&](int i) {
        return padL + (double)i / (historial.size() - 1) * iW;
    };
    auto toY = [&](long long v) {
        return padT + (1.0 - (double)(v - minV) / (maxV - minV)) * iH;
    };

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='" << H << "'>\n";
    svg << "<rect width='" << W << "' height='" << H << "' fill='#181c27'/>\n";

    // Título
    svg << "<text x='" << W/2 << "' y='22' text-anchor='middle' "
        << "font-family='monospace' font-size='13' font-weight='bold' fill='#e8eaf0'>"
        << "Curva de Convergencia ABC - " << etiquetaInstancia << "</text>\n";

    // Grid horizontal
    for (int t = 0; t <= 4; t++) {
        double y = padT + (double)t / 4 * iH;
        long long val = maxV - (long long)((double)t / 4 * (maxV - minV));
        svg << "<line x1='" << padL << "' y1='" << y << "' x2='" << padL+iW
            << "' y2='" << y << "' stroke='#252b3b' stroke-width='1'/>\n";
        svg << "<text x='" << padL-6 << "' y='" << y+4
            << "' text-anchor='end' font-family='monospace' font-size='10' fill='#6b7494'>"
            << val << "</text>\n";
    }

    // Grid vertical + labels eje X
    for (int t = 0; t <= 4; t++) {
        double x = padL + (double)t / 4 * iW;
        int iter = (int)((double)t / 4 * (historial.size() - 1)) + 1;
        svg << "<line x1='" << x << "' y1='" << padT << "' x2='" << x
            << "' y2='" << padT+iH << "' stroke='#252b3b' stroke-width='1'/>\n";
        svg << "<text x='" << x << "' y='" << padT+iH+16
            << "' text-anchor='middle' font-family='monospace' font-size='10' fill='#6b7494'>"
            << iter << "</text>\n";
    }

    // Labels ejes
    svg << "<text x='" << padL + iW/2 << "' y='" << H-6
        << "' text-anchor='middle' font-family='monospace' font-size='11' fill='#6b7494'>"
        << "Iteracion</text>\n";
    svg << "<text x='14' y='" << padT + iH/2
        << "' text-anchor='middle' font-family='monospace' font-size='11' fill='#6b7494' "
        << "transform='rotate(-90,14," << padT + iH/2 << ")'>"
        << "Mejor Makespan</text>\n";

    // Área bajo la curva
    svg << "<polyline points='";
    for (int i = 0; i < (int)historial.size(); i++)
        svg << toX(i) << "," << toY(historial[i]) << " ";
    svg << toX(historial.size()-1) << "," << padT+iH << " "
        << padL << "," << padT+iH
        << "' fill='#f5c84233' stroke='none'/>\n";

    // Línea de convergencia
    svg << "<polyline fill='none' stroke='#f5c842' stroke-width='2' points='";
    for (int i = 0; i < (int)historial.size(); i++)
        svg << toX(i) << "," << toY(historial[i]) << " ";
    svg << "'/>\n";

    // Punto y valor final
    double xFin = toX(historial.size()-1);
    double yFin = toY(historial.back());
    svg << "<circle cx='" << xFin << "' cy='" << yFin
        << "' r='4' fill='#f5c842'/>\n";
    svg << "<text x='" << xFin+8 << "' y='" << yFin+4
        << "' font-family='monospace' font-size='11' fill='#f5c842'>"
        << historial.back() << "</text>\n";

    svg << "</svg>\n";
    svg.close();
}

// Genera la grafica SVG del diagrama de Gantt de una secuencia dada.
// Tambien extraida a funcion independiente por el mismo motivo que la
// curva de convergencia: reutilizarla desde el modo --experimento.
void generarGraficaGanttSVG(
    const string& archivoSVG,
    const string& etiquetaInstancia,
    const Instancia& instancia,
    const vector<int>& mejorOrden,
    long long makespan,
    const vector<vector<long long>>& tablaFinalizacion
) {
    ofstream svg(archivoSVG);
    if (!svg.is_open()) {
        cerr << "Advertencia: no se pudo crear " << archivoSVG << endl;
        return;
    }

    int n = instancia.trabajos;
    int m = instancia.maquinas;

    const int W     = 900;
    const int padL  = 60, padR = 30, padT = 60, padB = 40;
    const int rowH  = 48;
    int H = padT + m * rowH + padB;
    int iW = W - padL - padR;

    // Paleta de colores para los trabajos
    vector<string> paleta = {
        "#f5c842","#4ecdc4","#ff6b6b","#a78bfa","#34d399",
        "#fb923c","#60a5fa","#f472b6","#a3e635","#e879f9",
        "#fbbf24","#2dd4bf","#f87171","#818cf8","#4ade80",
        "#facc15","#22d3ee","#fb7185","#c084fc","#86efac"
    };

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='" << H << "'>\n";
    svg << "<rect width='" << W << "' height='" << H << "' fill='#181c27'/>\n";

    // Título
    svg << "<text x='" << W/2 << "' y='20' text-anchor='middle' "
        << "font-family='monospace' font-size='13' font-weight='bold' fill='#e8eaf0'>"
        << "Diagrama de Gantt - " << etiquetaInstancia << "</text>\n";
    svg << "<text x='" << W/2 << "' y='38' text-anchor='middle' "
        << "font-family='monospace' font-size='11' fill='#aab0c4'>"
        << "Secuencia: " << ordenAString(mejorOrden)
        << "  |  Makespan = " << makespan << "</text>\n";

    // Fondo de filas alternas
    for (int j = 0; j < m; j++) {
        int y = padT + j * rowH;
        string bg = (j % 2 == 0) ? "#181c27" : "#1c2130";
        svg << "<rect x='" << padL << "' y='" << y << "' width='" << iW
            << "' height='" << rowH << "' fill='" << bg << "'/>\n";
    }

    // Grid vertical
    int paso = max(1LL, makespan / 8);
    for (long long t = 0; t <= makespan; t += paso) {
        double x = padL + (double)t / makespan * iW;
        svg << "<line x1='" << x << "' y1='" << padT << "' x2='" << x
            << "' y2='" << padT + m*rowH << "' stroke='#252b3b' stroke-width='1'/>\n";
        svg << "<text x='" << x << "' y='" << padT + m*rowH + 16
            << "' text-anchor='middle' font-family='monospace' font-size='9' fill='#6b7494'>"
            << t << "</text>\n";
    }

    // Labels máquinas
    for (int j = 0; j < m; j++) {
        int y = padT + j * rowH + rowH / 2 + 4;
        svg << "<text x='" << padL - 8 << "' y='" << y
            << "' text-anchor='end' font-family='monospace' font-size='11' fill='#e8eaf0'>"
            << "M" << j+1 << "</text>\n";
    }

    // Barras del Gantt
    const int barPad = 6;
    for (int i = 0; i < n; i++) {
        int trabajo = mejorOrden[i];
        string color = paleta[trabajo % paleta.size()];

        for (int j = 0; j < m; j++) {
            long long fin = tablaFinalizacion[i][j];
            int dur       = instancia.tiempos[trabajo][j];
            long long ini = fin - dur;

            double x  = padL + (double)ini / makespan * iW;
            double bW = (double)dur / makespan * iW;
            double y  = padT + j * rowH + barPad;
            double bH = rowH - barPad * 2;

            // Sombra
            svg << "<rect x='" << x+2 << "' y='" << y+2
                << "' width='" << bW << "' height='" << bH
                << "' fill='#000000' opacity='0.4' rx='2'/>\n";
            // Barra
            svg << "<rect x='" << x << "' y='" << y
                << "' width='" << bW << "' height='" << bH
                << "' fill='" << color << "' rx='2'/>\n";
            // Etiqueta dentro de la barra
            if (bW > 20) {
                svg << "<text x='" << x + bW/2 << "' y='" << y + bH/2 + 4
                    << "' text-anchor='middle' font-family='monospace' "
                    << "font-size='9' font-weight='bold' fill='#000000CC'>"
                    << "J" << trabajo+1 << "</text>\n";
            }
        }
    }

    // Línea de makespan
    double xEnd = padL + iW;
    svg << "<line x1='" << xEnd << "' y1='" << padT - 10 << "' x2='" << xEnd
        << "' y2='" << padT + m*rowH
        << "' stroke='#f5c842' stroke-width='2' stroke-dasharray='5,3'/>\n";
    svg << "<text x='" << xEnd - 4 << "' y='" << padT - 14
        << "' text-anchor='end' font-family='monospace' font-size='10' fill='#f5c842'>"
        << "Cmax=" << makespan << "</text>\n";

    // Leyenda
    int legX = padL;
    int legY = padT + m * rowH + 28;
    for (int i = 0; i < n && i < 10; i++) {
        int trabajo = mejorOrden[i];
        string color = paleta[trabajo % paleta.size()];
        svg << "<rect x='" << legX << "' y='" << legY - 10
            << "' width='12' height='12' fill='" << color << "' rx='2'/>\n";
        svg << "<text x='" << legX + 16 << "' y='" << legY
            << "' font-family='monospace' font-size='10' fill='#aab0c4'>"
            << "J" << trabajo+1 << "</text>\n";
        legX += 48;
    }

    // Label eje X
    svg << "<text x='" << padL + iW/2 << "' y='" << H - 4
        << "' text-anchor='middle' font-family='monospace' font-size='11' fill='#6b7494'>"
        << "Tiempo</text>\n";

    svg << "</svg>\n";
    svg.close();
}

// ----------------------------------------------------------------------
// HEURISTICA BASE DE COMPARACION: NEH (Nawaz, Enscore y Ham, 1983)
// ----------------------------------------------------------------------
// El proyecto pide comparar el algoritmo metaheuristico contra una
// heuristica base (ver seccion 7 del enunciado, "presentacion oral").
// Se usa NEH porque es la heuristica constructiva de referencia mas citada
// para el PFSP y suele dar muy buenos resultados con poco costo
// computacional, lo que la hace un punto de comparacion mas exigente
// (y mas creible en la exposicion) que una simple secuencia aleatoria.
//
// Pasos del algoritmo:
//   1. Calcular la suma de tiempos de procesamiento de cada trabajo.
//   2. Ordenar los trabajos de mayor a menor segun esa suma.
//   3. Insertar los trabajos uno a uno, en ese orden, probando todas las
//      posiciones posibles dentro de la secuencia parcial y quedandose
//      con la posicion que produce el menor makespan.
vector<int> heuristicaNEH(const Instancia& instancia) {
    int n = instancia.trabajos;
    int m = instancia.maquinas;

    // 1. Suma de tiempos de procesamiento por trabajo.
    vector<long long> sumaTiempos(n, 0);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            sumaTiempos[i] += instancia.tiempos[i][j];
        }
    }

    // 2. Orden inicial de trabajos por suma de tiempos descendente.
    vector<int> ordenInicial = generarOrdenInicial(n);
    sort(ordenInicial.begin(), ordenInicial.end(), [&](int a, int b) {
        return sumaTiempos[a] > sumaTiempos[b];
    });

    if (n == 0) return {};

    // 3. Insercion voraz: se construye la secuencia final probando, para
    // cada trabajo nuevo, todas las posiciones posibles y eligiendo la
    // que da el menor makespan parcial.
    vector<int> secuencia;
    secuencia.push_back(ordenInicial[0]);

    for (int k = 1; k < n; k++) {
        int trabajoNuevo = ordenInicial[k];

        long long mejorMakespan = LLONG_MAX;
        int mejorPosicion = 0;

        for (int pos = 0; pos <= (int)secuencia.size(); pos++) {
            vector<int> candidato = secuencia;
            candidato.insert(candidato.begin() + pos, trabajoNuevo);

            long long ms = calcularMakespan(instancia, candidato);
            if (ms < mejorMakespan) {
                mejorMakespan = ms;
                mejorPosicion = pos;
            }
        }

        secuencia.insert(secuencia.begin() + mejorPosicion, trabajoNuevo);
    }

    return secuencia;
}

// ----------------------------------------------------------------------
// PARTE 2 (Opcion 3): algoritmo ABC adaptado al PFSP
// ----------------------------------------------------------------------

struct FuenteAlimento {
    vector<int> orden;       // permutacion de trabajos (fuente de alimento)
    long long makespan;      // calidad de la solucion (menor es mejor)
    int intentos;            // contador de intentos sin mejora (para fase exploradora)
};

struct ParametrosABC {
    int numAbejas;      // numero de fuentes de alimento (= abejas empleadas)
    int limite;          // limite de intentos antes de abandonar una fuente
    int maxIteraciones;  // criterio de parada
    unsigned int semilla;
};

mt19937 rng;

// Fitness usado para la seleccion por ruleta en la fase observadora.
// A menor makespan, mayor fitness.
double calcularFitness(long long makespan) {
    return 1.0 / (1.0 + (double)makespan);
}

// Genera una permutacion aleatoria de trabajos (solucion inicial al azar).
vector<int> generarOrdenAleatorio(int cantidadTrabajos) {
    vector<int> orden = generarOrdenInicial(cantidadTrabajos);
    shuffle(orden.begin(), orden.end(), rng);
    return orden;
}

// Operador de intercambio (swap): intercambia dos posiciones al azar.
vector<int> operadorSwap(const vector<int>& orden) {
    vector<int> nuevo = orden;
    int n = nuevo.size();
    if (n < 2) return nuevo;

    uniform_int_distribution<int> dist(0, n - 1);
    int a = dist(rng);
    int b = dist(rng);
    while (b == a) b = dist(rng);

    swap(nuevo[a], nuevo[b]);
    return nuevo;
}

// Operador de insercion: saca un trabajo de una posicion y lo inserta en otra.
vector<int> operadorInsercion(const vector<int>& orden) {
    vector<int> nuevo = orden;
    int n = nuevo.size();
    if (n < 2) return nuevo;

    uniform_int_distribution<int> dist(0, n - 1);
    int origen = dist(rng);
    int destino = dist(rng);
    while (destino == origen) destino = dist(rng);

    int trabajo = nuevo[origen];
    nuevo.erase(nuevo.begin() + origen);
    nuevo.insert(nuevo.begin() + destino, trabajo);

    return nuevo;
}

// Genera una solucion vecina aplicando al azar uno de los dos operadores
// (intercambio o insercion), tal como se definio en la adaptacion del
// algoritmo ABC al PFSP (Opcion 2).
vector<int> generarVecino(const vector<int>& orden) {
    uniform_int_distribution<int> moneda(0, 1);
    if (moneda(rng) == 0) {
        return operadorSwap(orden);
    } else {
        return operadorInsercion(orden);
    }
}

// Inicializa las fuentes de alimento (poblacion inicial).
vector<FuenteAlimento> inicializarFuentes(
    const Instancia& instancia,
    int numAbejas
) {
    vector<FuenteAlimento> fuentes(numAbejas);

    for (int i = 0; i < numAbejas; i++) {
        fuentes[i].orden = generarOrdenAleatorio(instancia.trabajos);
        fuentes[i].makespan = calcularMakespan(instancia, fuentes[i].orden);
        fuentes[i].intentos = 0;
    }

    return fuentes;
}

// Aplica seleccion voraz (greedy): si la nueva solucion es mejor o igual,
// reemplaza a la fuente y reinicia su contador de intentos; si no,
// se conserva la fuente anterior y se incrementa el contador de intentos.
void seleccionVoraz(FuenteAlimento& fuente, const vector<int>& nuevoOrden, long long nuevoMakespan) {
    if (nuevoMakespan < fuente.makespan) {
        fuente.orden = nuevoOrden;
        fuente.makespan = nuevoMakespan;
        fuente.intentos = 0;
    } else {
        fuente.intentos++;
    }
}

// Fase de abejas empleadas: cada abeja empleada explora un vecino de su
// propia fuente de alimento y aplica seleccion voraz.
void faseEmpleadas(vector<FuenteAlimento>& fuentes, const Instancia& instancia) {
    for (auto& fuente : fuentes) {
        vector<int> vecino = generarVecino(fuente.orden);
        long long makespanVecino = calcularMakespan(instancia, vecino);
        seleccionVoraz(fuente, vecino, makespanVecino);
    }
}

// Fase de abejas observadoras: cada observadora elige una fuente mediante
// seleccion por ruleta (proporcional al fitness), explora un vecino y
// aplica seleccion voraz. Esto concentra la busqueda en las mejores fuentes.
void faseObservadoras(vector<FuenteAlimento>& fuentes, const Instancia& instancia) {
    int numAbejas = fuentes.size();

    vector<double> fitness(numAbejas);
    double sumaFitness = 0.0;
    for (int i = 0; i < numAbejas; i++) {
        fitness[i] = calcularFitness(fuentes[i].makespan);
        sumaFitness += fitness[i];
    }

    uniform_real_distribution<double> distProb(0.0, 1.0);

    int i = 0;
    int observadorasAsignadas = 0;

    while (observadorasAsignadas < numAbejas) {
        double r = distProb(rng);
        double probabilidad = fitness[i] / sumaFitness;

        if (r < probabilidad) {
            vector<int> vecino = generarVecino(fuentes[i].orden);
            long long makespanVecino = calcularMakespan(instancia, vecino);
            seleccionVoraz(fuentes[i], vecino, makespanVecino);

            // El fitness pudo cambiar tras la seleccion voraz; se actualiza.
            fitness[i] = calcularFitness(fuentes[i].makespan);

            observadorasAsignadas++;
        }

        i = (i + 1) % numAbejas;
    }
}

// Fase de abejas exploradoras: si una fuente supera el limite de intentos
// sin mejorar, se considera agotada y se reemplaza por una nueva fuente
// generada al azar (la abeja empleada se convierte en exploradora).
void faseExploradoras(
    vector<FuenteAlimento>& fuentes,
    const Instancia& instancia,
    const ParametrosABC& parametros,
    int& contadorReinicios
) {
    for (auto& fuente : fuentes) {
        if (fuente.intentos > parametros.limite) {
            fuente.orden = generarOrdenAleatorio(instancia.trabajos);
            fuente.makespan = calcularMakespan(instancia, fuente.orden);
            fuente.intentos = 0;
            contadorReinicios++;
        }
    }
}

// Estructura con el resultado completo de una ejecucion del algoritmo ABC.
struct ResultadoABC {
    vector<int> mejorOrden;
    long long mejorMakespan;
    int iteracionesEjecutadas;
    int reiniciosExploradoras;
    double tiempoEjecucionSegundos;
    vector<long long> historialMejorMakespan; // para la curva de convergencia
};

// Ciclo principal del algoritmo ABC aplicado al PFSP.
ResultadoABC ejecutarABC(const Instancia& instancia, const ParametrosABC& parametros) {
    rng.seed(parametros.semilla);

    auto inicio = high_resolution_clock::now();

    vector<FuenteAlimento> fuentes = inicializarFuentes(instancia, parametros.numAbejas);

    // Mejor solucion global encontrada hasta el momento.
    int mejorIdx = 0;
    for (int i = 1; i < parametros.numAbejas; i++) {
        if (fuentes[i].makespan < fuentes[mejorIdx].makespan) mejorIdx = i;
    }
    vector<int> mejorOrdenGlobal = fuentes[mejorIdx].orden;
    long long mejorMakespanGlobal = fuentes[mejorIdx].makespan;

    vector<long long> historial;
    historial.reserve(parametros.maxIteraciones);

    int contadorReinicios = 0;
    int iteracion = 0;

    for (iteracion = 1; iteracion <= parametros.maxIteraciones; iteracion++) {
        // 1. Fase de abejas empleadas
        faseEmpleadas(fuentes, instancia);

        // 2. Fase de abejas observadoras
        faseObservadoras(fuentes, instancia);

        // 3. Actualizar la mejor solucion encontrada hasta el momento
        for (const auto& fuente : fuentes) {
            if (fuente.makespan < mejorMakespanGlobal) {
                mejorMakespanGlobal = fuente.makespan;
                mejorOrdenGlobal = fuente.orden;
            }
        }

        // 4. Fase de abejas exploradoras
        faseExploradoras(fuentes, instancia, parametros, contadorReinicios);

        // Registrar evolucion del mejor makespan para la curva de convergencia
        historial.push_back(mejorMakespanGlobal);
    }

    auto fin = high_resolution_clock::now();
    double segundos = duration_cast<duration<double>>(fin - inicio).count();

    ResultadoABC resultado;
    resultado.mejorOrden = mejorOrdenGlobal;
    resultado.mejorMakespan = mejorMakespanGlobal;
    resultado.iteracionesEjecutadas = iteracion - 1;
    resultado.reiniciosExploradoras = contadorReinicios;
    resultado.tiempoEjecucionSegundos = segundos;
    resultado.historialMejorMakespan = historial;

    return resultado;
}

// ----------------------------------------------------------------------
// Salida de resultados
// ----------------------------------------------------------------------

void mostrarMatriz(const Instancia& instancia) {
    cout << "\nMatriz de tiempos de procesamiento:\n\n";
    cout << setw(10) << "Trabajo";
    for (int j = 0; j < instancia.maquinas; j++) cout << setw(8) << ("M" + to_string(j + 1));
    cout << endl;

    for (int i = 0; i < instancia.trabajos; i++) {
        cout << setw(10) << ("J" + to_string(i + 1));
        for (int j = 0; j < instancia.maquinas; j++) cout << setw(8) << instancia.tiempos[i][j];
        cout << endl;
    }
}

// Muestra en consola la tabla de tiempos de finalizacion C[i][j] de la mejor
// secuencia encontrada (la misma informacion que se exporta a CSV), para que
// el resultado tambien sea visible directamente al ejecutar el programa.
void mostrarTablaFinalizacion(
    const vector<vector<long long>>& C,
    const vector<int>& ordenTrabajos
) {
    if (C.empty()) return;
    int m = (int)C[0].size();

    cout << "\nTabla de tiempos de finalizacion C[i][j] (mejor secuencia):\n\n";
    cout << setw(10) << "Trabajo";
    for (int j = 0; j < m; j++) cout << setw(10) << ("M" + to_string(j + 1));
    cout << endl;

    for (int i = 0; i < (int)C.size(); i++) {
        cout << setw(10) << ("J" + to_string(ordenTrabajos[i] + 1));
        for (int j = 0; j < m; j++) cout << setw(10) << C[i][j];
        cout << endl;
    }
}

void exportarResumenABC_TXT(
    const string& nombreArchivoSalida,
    const string& nombreInstancia,
    const Instancia& instancia,
    const ParametrosABC& parametros,
    const ResultadoABC& resultado,
    long long makespanNEH,
    const vector<int>& secuenciaNEH
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Resumen de ejecucion - Algoritmo ABC para PFSP\n";
    salida << "Instancia: " << nombreInstancia << "\n";
    salida << "Formato de archivo detectado: " << instancia.formatoDetectado << "\n";
    salida << "Trabajos: " << instancia.trabajos << "\n";
    salida << "Maquinas: " << instancia.maquinas << "\n\n";

    salida << "Parametros utilizados:\n";
    salida << "  Numero de abejas (fuentes): " << parametros.numAbejas << "\n";
    salida << "  Limite de intentos (abandono): " << parametros.limite << "\n";
    salida << "  Maximo de iteraciones: " << parametros.maxIteraciones << "\n";
    salida << "  Semilla aleatoria: " << parametros.semilla << "\n\n";

    salida << "Resultados ABC:\n";
    salida << "  Mejor secuencia encontrada: " << ordenAString(resultado.mejorOrden) << "\n";
    salida << "  Mejor makespan: " << resultado.mejorMakespan << "\n";
    salida << "  Iteraciones ejecutadas: " << resultado.iteracionesEjecutadas << "\n";
    salida << "  Reinicios por fase exploradora: " << resultado.reiniciosExploradoras << "\n";
    salida << "  Tiempo de ejecucion (segundos): " << resultado.tiempoEjecucionSegundos << "\n\n";

    salida << "Comparacion con heuristica base (NEH):\n";
    salida << "  Secuencia NEH: " << ordenAString(secuenciaNEH) << "\n";
    salida << "  Makespan NEH: " << makespanNEH << "\n";
    if (makespanNEH > 0) {
        double mejoraPorcentual = 100.0 * (double)(makespanNEH - resultado.mejorMakespan) / (double)makespanNEH;
        salida << "  Mejora del ABC respecto a NEH: " << fixed << setprecision(2)
               << mejoraPorcentual << " %\n";
    }

    salida.close();
}

// Datos registrados de una ejecucion dentro del modo --experimento.
// El enunciado (seccion 5.1) pide registrar, por cada ejecucion: mejor
// secuencia, makespan, tiempo, iteraciones y parametros usados.
struct EjecucionExperimento {
    int numero;
    unsigned int semilla;
    vector<int> secuencia;
    long long makespan;
    double tiempoSegundos;
    int iteraciones;
};

// Guarda el detalle de TODAS las ejecuciones del experimento en un CSV
// (antes esta informacion solo se mostraba en consola y se perdia al
// cerrar la terminal).
void exportarExperimentoCSV(
    const string& nombreArchivoSalida,
    const vector<EjecucionExperimento>& ejecuciones
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Ejecucion,Semilla,Makespan,Tiempo_segundos,Iteraciones,Secuencia\n";
    for (const auto& e : ejecuciones) {
        salida << e.numero << "," << e.semilla << "," << e.makespan << ","
               << e.tiempoSegundos << "," << e.iteraciones << ","
               << ordenAString(e.secuencia) << "\n";
    }

    salida.close();
}

// Guarda el resumen estadistico del experimento (la tabla pedida en la
// seccion 5.2 del enunciado) junto con la comparacion contra NEH.
void exportarResumenExperimentoTXT(
    const string& nombreArchivoSalida,
    const string& nombreInstancia,
    const Instancia& instancia,
    const ParametrosABC& parametros,
    const vector<EjecucionExperimento>& ejecuciones,
    long long mejorMakespan,
    long long peorMakespan,
    double promedio,
    double desviacion,
    double tiempoPromedio,
    const vector<int>& mejorSecuencia,
    long long makespanNEH,
    const vector<int>& secuenciaNEH
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Resumen de experimento - Algoritmo ABC para PFSP\n";
    salida << "Instancia: " << nombreInstancia << "\n";
    salida << "Formato de archivo detectado: " << instancia.formatoDetectado << "\n";
    salida << "Trabajos: " << instancia.trabajos << "\n";
    salida << "Maquinas: " << instancia.maquinas << "\n\n";

    salida << "Parametros utilizados (constantes en las " << ejecuciones.size() << " ejecuciones):\n";
    salida << "  Numero de abejas (fuentes): " << parametros.numAbejas << "\n";
    salida << "  Limite de intentos (abandono): " << parametros.limite << "\n";
    salida << "  Maximo de iteraciones: " << parametros.maxIteraciones << "\n\n";

    salida << "Tabla resumen (seccion 5.2 del enunciado):\n";
    salida << "  Mejor makespan:      " << mejorMakespan << "\n";
    salida << "  Peor makespan:       " << peorMakespan << "\n";
    salida << "  Promedio:            " << fixed << setprecision(2) << promedio << "\n";
    salida << "  Desviacion estandar: " << fixed << setprecision(2) << desviacion << "\n";
    salida << "  Tiempo promedio (s): " << fixed << setprecision(6) << tiempoPromedio << "\n";
    salida << "  Mejor secuencia:     " << ordenAString(mejorSecuencia) << "\n\n";

    salida << "Comparacion con heuristica base (NEH):\n";
    salida << "  Secuencia NEH: " << ordenAString(secuenciaNEH) << "\n";
    salida << "  Makespan NEH:  " << makespanNEH << "\n";
    if (makespanNEH > 0) {
        double mejoraPorcentual = 100.0 * (double)(makespanNEH - mejorMakespan) / (double)makespanNEH;
        salida << "  Mejora del ABC (mejor de " << ejecuciones.size() << " corridas) respecto a NEH: "
               << fixed << setprecision(2) << mejoraPorcentual << " %\n";
    }

    salida << "\nDetalle por ejecucion (ver tambien el CSV adjunto):\n";
    for (const auto& e : ejecuciones) {
        salida << "  Ejecucion " << e.numero << " | semilla=" << e.semilla
               << " | makespan=" << e.makespan
               << " | tiempo=" << fixed << setprecision(6) << e.tiempoSegundos << "s"
               << " | iteraciones=" << e.iteraciones
               << " | secuencia=" << ordenAString(e.secuencia) << "\n";
    }

    salida.close();
}

// ----------------------------------------------------------------------
// main
// ----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Uso:" << endl;
        cout << "  " << argv[0] << " archivo_instancia.txt [numAbejas] [limite] [maxIteraciones] [semilla]" << endl;
        cout << "  " << argv[0] << " --experimento archivo_instancia.txt [numAbejas] [limite] [maxIteraciones]" << endl;
        cout << "\nValores por defecto: numAbejas=20 limite=30 maxIteraciones=200 semilla=aleatoria" << endl;
        return 1;
    }

    // ── Modo experimento: 10 ejecuciones con estadísticas ──────────────────
    if (string(argv[1]) == "--experimento" && argc >= 3) {
        string archivoInst = argv[2];
        Instancia instancia = leerInstancia(archivoInst);

        int numAbejas      = (argc > 3) ? atoi(argv[3]) : 20;
        int limite         = (argc > 4) ? atoi(argv[4]) : 30;
        int maxIteraciones = (argc > 5) ? atoi(argv[5]) : 200;

        vector<unsigned int> semillas = {42, 123, 456, 789, 1001, 2024, 3141, 9999, 11, 777};

        cout << "\n========================================" << endl;
        cout << "EXPERIMENTO: " << archivoInst << endl;
        cout << "Formato de archivo detectado: " << instancia.formatoDetectado << endl;
        cout << "Trabajos: " << instancia.trabajos
             << " | Maquinas: " << instancia.maquinas << endl;
        cout << "Parametros: abejas=" << numAbejas
             << " limite=" << limite
             << " iteraciones=" << maxIteraciones << endl;
        cout << "========================================\n" << endl;

        // Heuristica base de comparacion (NEH). Se calcula una sola vez,
        // ya que es deterministica y no depende de la semilla aleatoria.
        vector<int> secuenciaNEH = heuristicaNEH(instancia);
        long long makespanNEH = calcularMakespan(instancia, secuenciaNEH);
        cout << "Heuristica base (NEH):" << endl;
        cout << "  Secuencia: ";
        mostrarOrden(secuenciaNEH);
        cout << "\n  Makespan NEH: " << makespanNEH << "\n" << endl;

        vector<EjecucionExperimento> ejecuciones;
        long long mejorGlobal  = LLONG_MAX;
        long long peorGlobal   = 0;
        vector<int> mejorSecuencia;
        ResultadoABC mejorResultado;  // se conserva para exportar Gantt/convergencia de la mejor corrida

        cout << left
             << setw(10) << "Ejecucion"
             << setw(10) << "Semilla"
             << setw(12) << "Makespan"
             << setw(14) << "Tiempo (s)"
             << setw(12) << "Iteraciones" << endl;
        cout << string(58, '-') << endl;

        for (int i = 0; i < (int)semillas.size(); i++) {
            ParametrosABC params;
            params.numAbejas      = numAbejas;
            params.limite         = limite;
            params.maxIteraciones = maxIteraciones;
            params.semilla        = semillas[i];

            ResultadoABC res = ejecutarABC(instancia, params);

            EjecucionExperimento e;
            e.numero         = i + 1;
            e.semilla        = semillas[i];
            e.secuencia      = res.mejorOrden;
            e.makespan       = res.mejorMakespan;
            e.tiempoSegundos = res.tiempoEjecucionSegundos;
            e.iteraciones    = res.iteracionesEjecutadas;
            ejecuciones.push_back(e);

            if (res.mejorMakespan < mejorGlobal) {
                mejorGlobal    = res.mejorMakespan;
                mejorSecuencia = res.mejorOrden;
                mejorResultado = res;
            }
            if (res.mejorMakespan > peorGlobal)
                peorGlobal = res.mejorMakespan;

            cout << left
                 << setw(10) << (i + 1)
                 << setw(10) << semillas[i]
                 << setw(12) << res.mejorMakespan
                 << setw(14) << fixed << setprecision(6) << res.tiempoEjecucionSegundos
                 << setw(12) << res.iteracionesEjecutadas
                 << endl;
        }

        double suma = 0;
        for (const auto& e : ejecuciones) suma += e.makespan;
        double promedio = suma / ejecuciones.size();

        double varianza = 0;
        for (const auto& e : ejecuciones) varianza += (e.makespan - promedio) * (e.makespan - promedio);
        double desviacion = sqrt(varianza / ejecuciones.size());

        double sumaTiempo = 0;
        for (const auto& e : ejecuciones) sumaTiempo += e.tiempoSegundos;
        double tiempoProm = sumaTiempo / ejecuciones.size();

        cout << string(58, '-') << endl;
        cout << "\n--- TABLA RESUMEN (seccion 5.2 del enunciado) ---" << endl;
        cout << "Mejor makespan:      " << mejorGlobal   << endl;
        cout << "Peor makespan:       " << peorGlobal    << endl;
        cout << "Promedio:            " << fixed << setprecision(2) << promedio   << endl;
        cout << "Desviacion estandar: " << fixed << setprecision(2) << desviacion << endl;
        cout << "Tiempo promedio:     " << fixed << setprecision(6) << tiempoProm << " s" << endl;
        cout << "Mejor secuencia:     ";
        mostrarOrden(mejorSecuencia);
        cout << endl;

        double mejoraPorcentual = 100.0 * (double)(makespanNEH - mejorGlobal) / (double)makespanNEH;
        cout << "\nComparacion con heuristica base (NEH):" << endl;
        cout << "  Makespan NEH:            " << makespanNEH << endl;
        cout << "  Mejor makespan ABC:      " << mejorGlobal << endl;
        cout << "  Mejora del ABC vs. NEH:  " << fixed << setprecision(2) << mejoraPorcentual << " %" << endl;

        // ── Exportar TODO el detalle del experimento a archivo ──────────────
        // (antes esta informacion solo se imprimia en consola y se perdia).
        string base = obtenerNombreBase(archivoInst);
        string archivoExpCSV     = "salida_experimento_" + base + "_ejecuciones.csv";
        string archivoExpResumen = "salida_experimento_" + base + "_resumen.txt";

        exportarExperimentoCSV(archivoExpCSV, ejecuciones);
        exportarResumenExperimentoTXT(
            archivoExpResumen, archivoInst, instancia,
            ParametrosABC{numAbejas, limite, maxIteraciones, 0},
            ejecuciones, mejorGlobal, peorGlobal, promedio, desviacion, tiempoProm,
            mejorSecuencia, makespanNEH, secuenciaNEH
        );

        // También se generan la curva de convergencia y el Gantt de la
        // MEJOR corrida del experimento, igual que en el modo normal,
        // para que --experimento por si solo ya entregue todo lo pedido
        // en la seccion 5 (tabla, curva de convergencia y diagrama de Gantt).
        vector<vector<long long>> tablaFinalizacionMejor =
            generarTablaFinalizacion(instancia, mejorResultado.mejorOrden);

        string archivoExpFinalizacion = "salida_experimento_" + base + "_tabla_finalizacion.csv";
        string archivoExpGanttCSV     = "salida_experimento_" + base + "_datos_gantt.csv";
        string archivoExpConvergencia = "salida_experimento_" + base + "_convergencia.csv";
        string archivoExpSvgConverg   = "grafica_convergencia_experimento_" + base + ".svg";
        string archivoExpSvgGantt     = "grafica_gantt_experimento_" + base + ".svg";

        exportarTablaFinalizacionCSV(archivoExpFinalizacion, tablaFinalizacionMejor, mejorResultado.mejorOrden);
        exportarDatosGanttCSV(archivoExpGanttCSV, instancia, mejorResultado.mejorOrden, tablaFinalizacionMejor);
        exportarConvergenciaCSV(archivoExpConvergencia, mejorResultado.historialMejorMakespan);
        generarGraficaConvergenciaSVG(archivoExpSvgConverg, archivoInst, mejorResultado.historialMejorMakespan);
        generarGraficaGanttSVG(archivoExpSvgGantt, archivoInst, instancia, mejorResultado.mejorOrden,
                                mejorResultado.mejorMakespan, tablaFinalizacionMejor);

        cout << "\nArchivos generados:" << endl;
        cout << " - " << archivoExpCSV     << " (detalle de las " << ejecuciones.size() << " ejecuciones)" << endl;
        cout << " - " << archivoExpResumen << " (tabla resumen + comparacion con NEH)" << endl;
        cout << " - " << archivoExpFinalizacion << endl;
        cout << " - " << archivoExpGanttCSV     << endl;
        cout << " - " << archivoExpConvergencia << " (de la mejor corrida)" << endl;
        cout << " - " << archivoExpSvgConverg   << " (curva de convergencia visual)" << endl;
        cout << " - " << archivoExpSvgGantt     << " (diagrama de Gantt visual)" << endl;
        cout << endl;

        return 0;
    }

    // ── Modo normal: una sola ejecucion ─────────────────────────────────────
    string nombreArchivo = argv[1];

    ParametrosABC parametros;
    parametros.numAbejas      = (argc > 2) ? atoi(argv[2]) : 20;
    parametros.limite         = (argc > 3) ? atoi(argv[3]) : 30;
    parametros.maxIteraciones = (argc > 4) ? atoi(argv[4]) : 200;
    parametros.semilla        = (argc > 5) ? (unsigned int)atoi(argv[5])
                                            : (unsigned int)chrono::system_clock::now().time_since_epoch().count();

    if (parametros.numAbejas < 2) {
        cerr << "Error: numAbejas debe ser al menos 2." << endl;
        return 1;
    }
    if (parametros.maxIteraciones < 1) {
        cerr << "Error: maxIteraciones debe ser al menos 1." << endl;
        return 1;
    }

    Instancia instancia = leerInstancia(nombreArchivo);

    cout << "\nArchivo cargado: " << nombreArchivo << endl;
    cout << "Formato de archivo detectado: " << instancia.formatoDetectado << endl;
    cout << "Cantidad de trabajos: " << instancia.trabajos << endl;
    cout << "Cantidad de maquinas: " << instancia.maquinas << endl;

    mostrarMatriz(instancia);

    cout << "\nParametros del algoritmo ABC:" << endl;
    cout << "  Numero de abejas (fuentes): " << parametros.numAbejas << endl;
    cout << "  Limite de intentos (abandono): " << parametros.limite << endl;
    cout << "  Maximo de iteraciones: " << parametros.maxIteraciones << endl;
    cout << "  Semilla aleatoria: " << parametros.semilla << endl;

    // Heuristica base de comparacion (NEH), calculada antes de correr ABC.
    vector<int> secuenciaNEH = heuristicaNEH(instancia);
    long long makespanNEH = calcularMakespan(instancia, secuenciaNEH);

    ResultadoABC resultado = ejecutarABC(instancia, parametros);

    if (!validarOrden(resultado.mejorOrden, instancia.trabajos)) {
        cerr << "Error interno: la mejor secuencia encontrada no es una permutacion valida." << endl;
        return 1;
    }

    cout << "\n================ RESULTADOS ================" << endl;
    cout << "Mejor secuencia encontrada: ";
    mostrarOrden(resultado.mejorOrden);
    cout << endl;
    cout << "Mejor makespan: " << resultado.mejorMakespan << endl;
    cout << "Iteraciones ejecutadas: " << resultado.iteracionesEjecutadas << endl;
    cout << "Reinicios por fase exploradora: " << resultado.reiniciosExploradoras << endl;
    cout << "Tiempo de ejecucion: " << resultado.tiempoEjecucionSegundos << " segundos" << endl;
    cout << "==============================================" << endl;

    cout << "\n---- Comparacion con heuristica base (NEH) ----" << endl;
    cout << "Secuencia NEH: ";
    mostrarOrden(secuenciaNEH);
    cout << endl;
    cout << "Makespan NEH: " << makespanNEH << endl;
    if (makespanNEH > 0) {
        double mejoraPorcentual = 100.0 * (double)(makespanNEH - resultado.mejorMakespan) / (double)makespanNEH;
        cout << "Mejora del ABC respecto a NEH: " << fixed << setprecision(2) << mejoraPorcentual << " %" << endl;
    }
    cout << "------------------------------------------------" << endl;

    // Generar tabla de finalizacion y datos de Gantt de la mejor solucion,
    // reutilizando las funciones integradas de la Opcion 1.
    vector<vector<long long>> tablaFinalizacion =
        generarTablaFinalizacion(instancia, resultado.mejorOrden);

    // Tambien se muestra la tabla de finalizacion directamente en consola,
    // ademas de exportarla a CSV, para que el resultado sea visible sin
    // necesidad de abrir ningun archivo aparte.
    mostrarTablaFinalizacion(tablaFinalizacion, resultado.mejorOrden);

    string base = obtenerNombreBase(nombreArchivo);

    string archivoFinalizacion = "salida_abc_" + base + "_tabla_finalizacion.csv";
    string archivoGantt        = "salida_abc_" + base + "_datos_gantt.csv";
    string archivoConvergencia = "salida_abc_" + base + "_convergencia.csv";
    string archivoResumen      = "salida_abc_" + base + "_resumen.txt";
    string archivoSvgConverg   = "grafica_convergencia_" + base + ".svg";
    string archivoSvgGantt     = "grafica_gantt_" + base + ".svg";

    exportarTablaFinalizacionCSV(archivoFinalizacion, tablaFinalizacion, resultado.mejorOrden);
    exportarDatosGanttCSV(archivoGantt, instancia, resultado.mejorOrden, tablaFinalizacion);
    exportarConvergenciaCSV(archivoConvergencia, resultado.historialMejorMakespan);
    exportarResumenABC_TXT(archivoResumen, nombreArchivo, instancia, parametros, resultado, makespanNEH, secuenciaNEH);
    generarGraficaConvergenciaSVG(archivoSvgConverg, nombreArchivo, resultado.historialMejorMakespan);
    generarGraficaGanttSVG(archivoSvgGantt, nombreArchivo, instancia, resultado.mejorOrden,
                            resultado.mejorMakespan, tablaFinalizacion);

    cout << "\nArchivos generados:" << endl;
    cout << " - " << archivoFinalizacion << endl;
    cout << " - " << archivoGantt << endl;
    cout << " - " << archivoConvergencia << " (para la curva de convergencia)" << endl;
    cout << " - " << archivoResumen << " (incluye comparacion con NEH)" << endl;
    cout << " - " << archivoSvgConverg << " (curva de convergencia visual)" << endl;
    cout << " - " << archivoSvgGantt << " (diagrama de Gantt visual)" << endl;

    return 0;
}
