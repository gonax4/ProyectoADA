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
      - La lectura de instancias, la matriz de tiempos, la tabla de
        finalizacion y el calculo del makespan (Opcion 1).
      - El ciclo completo del algoritmo ABC adaptado al PFSP (Opcion 2):
          * Generacion de soluciones iniciales (fuentes de alimento)
          * Fase de abejas empleadas
          * Fase de abejas observadoras
          * Fase de abejas exploradoras
          * Actualizacion de la mejor solucion
      - Registro de la evolucion del mejor makespan por iteracion
        (curva de convergencia).
      - Generacion de los datos para el diagrama de Gantt de la mejor
        solucion encontrada.

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

    Ejemplo:
        ./abc_pfsp instancia1_bas1.txt 20 30 200 42

    Si no se indican los parametros opcionales, se usan valores por defecto.
*/

// ----------------------------------------------------------------------
// PARTE 1 (Opcion 1): lectura de instancias, makespan, tabla, Gantt
// ----------------------------------------------------------------------

struct Instancia {
    int trabajos;
    int maquinas;
    vector<vector<int>> tiempos;
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

    instancia.tiempos.assign(
        instancia.trabajos,
        vector<int>(instancia.maquinas, 0)
    );

    for (int i = 0; i < instancia.trabajos; i++) {
        for (int j = 0; j < instancia.maquinas; j++) {
            int maquina;
            int tiempo;

            if (!(archivo >> maquina >> tiempo)) {
                cerr << "Error: faltan datos en el archivo para el trabajo J" << i + 1 << "." << endl;
                exit(1);
            }

            if (maquina < 0 || maquina >= instancia.maquinas) {
                cerr << "Error: indice de maquina invalido en J" << i + 1 << ": " << maquina << endl;
                exit(1);
            }

            if (tiempo < 0) {
                cerr << "Error: tiempo de procesamiento negativo en J" << i + 1 << "." << endl;
                exit(1);
            }

            instancia.tiempos[i][maquina] = tiempo;
        }
    }

    archivo.close();

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

void exportarResumenABC_TXT(
    const string& nombreArchivoSalida,
    const string& nombreInstancia,
    const Instancia& instancia,
    const ParametrosABC& parametros,
    const ResultadoABC& resultado
) {
    ofstream salida(nombreArchivoSalida);
    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Resumen de ejecucion - Algoritmo ABC para PFSP\n";
    salida << "Instancia: " << nombreInstancia << "\n";
    salida << "Trabajos: " << instancia.trabajos << "\n";
    salida << "Maquinas: " << instancia.maquinas << "\n\n";

    salida << "Parametros utilizados:\n";
    salida << "  Numero de abejas (fuentes): " << parametros.numAbejas << "\n";
    salida << "  Limite de intentos (abandono): " << parametros.limite << "\n";
    salida << "  Maximo de iteraciones: " << parametros.maxIteraciones << "\n";
    salida << "  Semilla aleatoria: " << parametros.semilla << "\n\n";

    salida << "Resultados:\n";
    salida << "  Mejor secuencia encontrada: " << ordenAString(resultado.mejorOrden) << "\n";
    salida << "  Mejor makespan: " << resultado.mejorMakespan << "\n";
    salida << "  Iteraciones ejecutadas: " << resultado.iteracionesEjecutadas << "\n";
    salida << "  Reinicios por fase exploradora: " << resultado.reiniciosExploradoras << "\n";
    salida << "  Tiempo de ejecucion (segundos): " << resultado.tiempoEjecucionSegundos << "\n";

    salida.close();
}

// ----------------------------------------------------------------------
// main
// ----------------------------------------------------------------------

int main(int argc, char* argv[]) {
if (argc < 2) {
        cout << "Uso:" << endl;
        cout << "  " << argv[0] << " archivo_instancia.txt [numAbejas] [limite] [maxIteraciones] [semilla]" << endl;
        cout << "  " << argv[0] << " --experimento archivo_instancia.txt" << endl;
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
        cout << "Trabajos: " << instancia.trabajos
             << " | Maquinas: " << instancia.maquinas << endl;
        cout << "Parametros: abejas=" << numAbejas
             << " limite=" << limite
             << " iteraciones=" << maxIteraciones << endl;
        cout << "========================================\n" << endl;

        vector<long long> makespans;
        vector<double>    tiempos;
        long long mejorGlobal  = LLONG_MAX;
        long long peorGlobal   = 0;
        vector<int> mejorSecuencia;

        cout << left
             << setw(10) << "Ejecucion"
             << setw(10) << "Semilla"
             << setw(12) << "Makespan"
             << setw(14) << "Tiempo (s)" << endl;
        cout << string(46, '-') << endl;

        for (int i = 0; i < (int)semillas.size(); i++) {
            ParametrosABC params;
            params.numAbejas      = numAbejas;
            params.limite         = limite;
            params.maxIteraciones = maxIteraciones;
            params.semilla        = semillas[i];

            ResultadoABC res = ejecutarABC(instancia, params);

            makespans.push_back(res.mejorMakespan);
            tiempos.push_back(res.tiempoEjecucionSegundos);

            if (res.mejorMakespan < mejorGlobal) {
                mejorGlobal    = res.mejorMakespan;
                mejorSecuencia = res.mejorOrden;
            }
            if (res.mejorMakespan > peorGlobal)
                peorGlobal = res.mejorMakespan;

            cout << left
                 << setw(10) << (i + 1)
                 << setw(10) << semillas[i]
                 << setw(12) << res.mejorMakespan
                 << setw(14) << fixed << setprecision(6) << res.tiempoEjecucionSegundos
                 << endl;
        }

        double suma = 0;
        for (long long m : makespans) suma += m;
        double promedio = suma / makespans.size();

        double varianza = 0;
        for (long long m : makespans) varianza += (m - promedio) * (m - promedio);
        double desviacion = sqrt(varianza / makespans.size());

        double sumaTiempo = 0;
        for (double t : tiempos) sumaTiempo += t;
        double tiempoProm = sumaTiempo / tiempos.size();

        cout << string(46, '-') << endl;
        cout << "\n--- TABLA RESUMEN ---" << endl;
        cout << "Mejor makespan:      " << mejorGlobal   << endl;
        cout << "Peor makespan:       " << peorGlobal    << endl;
        cout << "Promedio:            " << fixed << setprecision(2) << promedio   << endl;
        cout << "Desviacion estandar: " << fixed << setprecision(2) << desviacion << endl;
        cout << "Tiempo promedio:     " << fixed << setprecision(6) << tiempoProm << " s" << endl;
        cout << "Mejor secuencia:     ";
        mostrarOrden(mejorSecuencia);
        cout << "\n" << endl;

        return 0;
    }

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
    cout << "Cantidad de trabajos: " << instancia.trabajos << endl;
    cout << "Cantidad de maquinas: " << instancia.maquinas << endl;

    mostrarMatriz(instancia);

    cout << "\nParametros del algoritmo ABC:" << endl;
    cout << "  Numero de abejas (fuentes): " << parametros.numAbejas << endl;
    cout << "  Limite de intentos (abandono): " << parametros.limite << endl;
    cout << "  Maximo de iteraciones: " << parametros.maxIteraciones << endl;
    cout << "  Semilla aleatoria: " << parametros.semilla << endl;

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

    // Generar tabla de finalizacion y datos de Gantt de la mejor solucion,
    // reutilizando las funciones integradas de la Opcion 1.
    vector<vector<long long>> tablaFinalizacion =
        generarTablaFinalizacion(instancia, resultado.mejorOrden);

    string base = obtenerNombreBase(nombreArchivo);

    string archivoFinalizacion = "salida_abc_" + base + "_tabla_finalizacion.csv";
    string archivoGantt        = "salida_abc_" + base + "_datos_gantt.csv";
    string archivoConvergencia = "salida_abc_" + base + "_convergencia.csv";
    string archivoResumen      = "salida_abc_" + base + "_resumen.txt";

    exportarTablaFinalizacionCSV(archivoFinalizacion, tablaFinalizacion, resultado.mejorOrden);
    exportarDatosGanttCSV(archivoGantt, instancia, resultado.mejorOrden, tablaFinalizacion);
    exportarConvergenciaCSV(archivoConvergencia, resultado.historialMejorMakespan);
    exportarResumenABC_TXT(archivoResumen, nombreArchivo, instancia, parametros, resultado);

    cout << "\nArchivos generados:" << endl;
    cout << " - " << archivoFinalizacion << endl;
    cout << " - " << archivoGantt << endl;
    cout << " - " << archivoConvergencia << " (para la curva de convergencia)" << endl;
    cout << " - " << archivoResumen << endl;

    // ── GRÁFICA: Curva de convergencia (SVG) ──────────────────────────────
{
    string archivoSVG = "grafica_convergencia_" + base + ".svg";
    ofstream svg(archivoSVG);

    const int W = 800, H = 300;
    const int padL = 70, padR = 30, padT = 40, padB = 50;
    int iW = W - padL - padR;
    int iH = H - padT - padB;

    const auto& hist = resultado.historialMejorMakespan;
    long long minV = *min_element(hist.begin(), hist.end());
    long long maxV = *max_element(hist.begin(), hist.end());
    if (maxV == minV) maxV = minV + 1;

    auto toX = [&](int i) {
        return padL + (double)i / (hist.size() - 1) * iW;
    };
    auto toY = [&](long long v) {
        return padT + (1.0 - (double)(v - minV) / (maxV - minV)) * iH;
    };

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='" << H << "'>\n";
    svg << "<rect width='" << W << "' height='" << H << "' fill='#181c27'/>\n";

    // Título
    svg << "<text x='" << W/2 << "' y='22' text-anchor='middle' "
        << "font-family='monospace' font-size='13' font-weight='bold' fill='#e8eaf0'>"
        << "Curva de Convergencia ABC - " << nombreArchivo << "</text>\n";

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
        int iter = (int)((double)t / 4 * (hist.size() - 1)) + 1;
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
    for (int i = 0; i < (int)hist.size(); i++)
        svg << toX(i) << "," << toY(hist[i]) << " ";
    svg << toX(hist.size()-1) << "," << padT+iH << " "
        << padL << "," << padT+iH
        << "' fill='#f5c84233' stroke='none'/>\n";

    // Línea de convergencia
    svg << "<polyline fill='none' stroke='#f5c842' stroke-width='2' points='";
    for (int i = 0; i < (int)hist.size(); i++)
        svg << toX(i) << "," << toY(hist[i]) << " ";
    svg << "'/>\n";

    // Punto y valor final
    double xFin = toX(hist.size()-1);
    double yFin = toY(hist.back());
    svg << "<circle cx='" << xFin << "' cy='" << yFin
        << "' r='4' fill='#f5c842'/>\n";
    svg << "<text x='" << xFin+8 << "' y='" << yFin+4
        << "' font-family='monospace' font-size='11' fill='#f5c842'>"
        << hist.back() << "</text>\n";

    svg << "</svg>\n";
    svg.close();
    cout << " - " << archivoSVG << " (curva de convergencia visual)" << endl;
}

// ── GRÁFICA: Diagrama de Gantt (SVG) ──────────────────────────────────
{
    string archivoSVG = "grafica_gantt_" + base + ".svg";
    ofstream svg(archivoSVG);

    int n = instancia.trabajos;
    int m = instancia.maquinas;
    long long makespan = resultado.mejorMakespan;

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
        << "Diagrama de Gantt - " << nombreArchivo << "</text>\n";
    svg << "<text x='" << W/2 << "' y='38' text-anchor='middle' "
        << "font-family='monospace' font-size='11' fill='#aab0c4'>"
        << "Secuencia: " << ordenAString(resultado.mejorOrden)
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
        int trabajo = resultado.mejorOrden[i];
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
        int trabajo = resultado.mejorOrden[i];
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
    cout << " - " << archivoSVG << " (diagrama de Gantt visual)" << endl;
}


    return 0;
}
