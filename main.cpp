#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

using namespace std;

/*
    Base para el problema PFSP (Permutation Flow Shop Scheduling Problem).

    Este archivo deja lista la parte inicial del proyecto:
    1. Lee una instancia .txt.
    2. Identifica cantidad de trabajos y máquinas.
    3. Guarda los tiempos de procesamiento en una matriz.
    4. Evalúa cualquier orden de trabajos.
    5. Calcula la tabla de tiempos de finalización.
    6. Calcula el makespan.
    7. Valida que el orden sea una permutación correcta.
    8. Genera datos de inicio y fin para elaborar el diagrama de Gantt.

    Uso:
        ./pfsp archivo_instancia.txt

    Uso con orden manual:
        ./pfsp archivo_instancia.txt 3 1 4 2 5

    Nota:
        El orden manual se escribe con trabajos desde 1, es decir, J1, J2, J3, etc.
        Internamente el programa usa índices desde 0.
*/

struct Instancia {
    int trabajos;
    int maquinas;
    vector<vector<int>> tiempos;
};

// Obtiene un nombre base simple para crear archivos de salida.
// Ejemplo: "instancia1_bas1.txt" -> "instancia1_bas1"
string obtenerNombreBase(const string& ruta) {
    size_t inicio = ruta.find_last_of("/\\");
    string nombre = (inicio == string::npos) ? ruta : ruta.substr(inicio + 1);

    size_t punto = nombre.find_last_of(".");
    if (punto != string::npos) {
        nombre = nombre.substr(0, punto);
    }

    return nombre;
}

// Lee una instancia PFSP desde un archivo .txt.
// El formato esperado es:
// primera línea: cantidad_trabajos cantidad_maquinas
// siguientes líneas: pares maquina tiempo para cada trabajo.
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

// Valida que el orden contenga todos los trabajos exactamente una vez.
// Ejemplo válido para 5 trabajos: {2, 0, 3, 1, 4}
// Representa: [J3, J1, J4, J2, J5]
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

// Genera un orden inicial simple: J1, J2, J3, ...
vector<int> generarOrdenInicial(int cantidadTrabajos) {
    vector<int> orden;

    for (int i = 0; i < cantidadTrabajos; i++) {
        orden.push_back(i);
    }

    return orden;
}

// Lee un orden manual desde la terminal.
// El usuario lo escribe como 1 2 3 4 5, pero internamente se guarda como 0 1 2 3 4.
vector<int> leerOrdenDesdeArgumentos(int argc, char* argv[], int cantidadTrabajos) {
    vector<int> orden;

    for (int i = 2; i < argc; i++) {
        int trabajo = atoi(argv[i]);
        orden.push_back(trabajo - 1);
    }

    if (orden.empty()) {
        return generarOrdenInicial(cantidadTrabajos);
    }

    return orden;
}

// Genera la tabla de tiempos de finalización C[i][j].
// C[i][j] indica cuándo termina el trabajo ubicado en la posición i del orden
// en la máquina j.
//
// Fórmula:
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

            long long finTrabajoAnterior = 0;
            long long finMaquinaAnterior = 0;

            if (i > 0) {
                finTrabajoAnterior = C[i - 1][j];
            }

            if (j > 0) {
                finMaquinaAnterior = C[i][j - 1];
            }

            C[i][j] = max(finTrabajoAnterior, finMaquinaAnterior) + tiempoProcesamiento;
        }
    }

    return C;
}

// Calcula el makespan de un orden de trabajos.
// El makespan es el último valor de la tabla de finalización.
long long calcularMakespan(
    const Instancia& instancia,
    const vector<int>& ordenTrabajos
) {
    vector<vector<long long>> C = generarTablaFinalizacion(instancia, ordenTrabajos);

    return C.back().back();
}

// Muestra un resumen de la instancia cargada.
void mostrarResumenInstancia(const string& nombreArchivo, const Instancia& instancia) {
    cout << "\nArchivo cargado: " << nombreArchivo << endl;
    cout << "Cantidad de trabajos: " << instancia.trabajos << endl;
    cout << "Cantidad de maquinas: " << instancia.maquinas << endl;
}

// Muestra la matriz de tiempos de procesamiento.
void mostrarMatriz(const Instancia& instancia) {
    cout << "\nMatriz de tiempos de procesamiento:\n\n";

    cout << setw(10) << "Trabajo";

    for (int j = 0; j < instancia.maquinas; j++) {
        cout << setw(8) << ("M" + to_string(j + 1));
    }

    cout << endl;

    for (int i = 0; i < instancia.trabajos; i++) {
        cout << setw(10) << ("J" + to_string(i + 1));

        for (int j = 0; j < instancia.maquinas; j++) {
            cout << setw(8) << instancia.tiempos[i][j];
        }

        cout << endl;
    }
}

// Muestra el orden de trabajos en formato [J1, J2, J3, ...].
void mostrarOrden(const vector<int>& ordenTrabajos) {
    cout << "[";

    for (size_t i = 0; i < ordenTrabajos.size(); i++) {
        cout << "J" << ordenTrabajos[i] + 1;

        if (i < ordenTrabajos.size() - 1) {
            cout << ", ";
        }
    }

    cout << "]";
}

// Muestra la tabla de tiempos de finalización.
void mostrarTablaFinalizacion(
    const vector<vector<long long>>& C,
    const vector<int>& ordenTrabajos
) {
    cout << "\nTabla de tiempos de finalizacion:\n\n";

    cout << setw(10) << "Trabajo";

    if (!C.empty()) {
        for (int j = 0; j < (int)C[0].size(); j++) {
            cout << setw(8) << ("M" + to_string(j + 1));
        }
    }

    cout << endl;

    for (int i = 0; i < (int)C.size(); i++) {
        cout << setw(10) << ("J" + to_string(ordenTrabajos[i] + 1));

        for (int j = 0; j < (int)C[i].size(); j++) {
            cout << setw(8) << C[i][j];
        }

        cout << endl;
    }
}

// Muestra los datos necesarios para elaborar un diagrama de Gantt.
// Para cada operación se obtiene:
// inicio = fin - duración.
void mostrarDatosGantt(
    const Instancia& instancia,
    const vector<int>& ordenTrabajos,
    const vector<vector<long long>>& C
) {
    cout << "\nDatos para diagrama de Gantt:\n\n";

    cout << setw(10) << "Trabajo"
         << setw(10) << "Maquina"
         << setw(10) << "Inicio"
         << setw(10) << "Fin"
         << setw(10) << "Duracion" << endl;

    for (int i = 0; i < (int)ordenTrabajos.size(); i++) {
        int trabajoActual = ordenTrabajos[i];

        for (int j = 0; j < instancia.maquinas; j++) {
            long long fin = C[i][j];
            int duracion = instancia.tiempos[trabajoActual][j];
            long long inicio = fin - duracion;

            cout << setw(10) << ("J" + to_string(trabajoActual + 1))
                 << setw(10) << ("M" + to_string(j + 1))
                 << setw(10) << inicio
                 << setw(10) << fin
                 << setw(10) << duracion << endl;
        }
    }
}

// Exporta la tabla de finalización a CSV.
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
        for (int j = 0; j < (int)C[0].size(); j++) {
            salida << ",M" << j + 1;
        }
    }

    salida << "\n";

    for (int i = 0; i < (int)C.size(); i++) {
        salida << "J" << ordenTrabajos[i] + 1;

        for (int j = 0; j < (int)C[i].size(); j++) {
            salida << "," << C[i][j];
        }

        salida << "\n";
    }

    salida.close();
}

// Exporta los datos para el diagrama de Gantt a CSV.
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

// Exporta un resumen simple del resultado.
void exportarResumenTXT(
    const string& nombreArchivoSalida,
    const string& nombreInstancia,
    const Instancia& instancia,
    const vector<int>& ordenTrabajos,
    long long makespan
) {
    ofstream salida(nombreArchivoSalida);

    if (!salida.is_open()) {
        cerr << "Advertencia: no se pudo crear " << nombreArchivoSalida << endl;
        return;
    }

    salida << "Resumen de evaluacion PFSP\n";
    salida << "Instancia: " << nombreInstancia << "\n";
    salida << "Trabajos: " << instancia.trabajos << "\n";
    salida << "Maquinas: " << instancia.maquinas << "\n";

    salida << "Orden evaluado: ";
    for (int i = 0; i < (int)ordenTrabajos.size(); i++) {
        salida << "J" << ordenTrabajos[i] + 1;
        if (i < (int)ordenTrabajos.size() - 1) {
            salida << " ";
        }
    }

    salida << "\nMakespan: " << makespan << "\n";

    salida.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Uso:" << endl;
        cout << "  " << argv[0] << " archivo_instancia.txt" << endl;
        cout << "  " << argv[0] << " archivo_instancia.txt 3 1 4 2 5" << endl;
        cout << "\nSi no se indica un orden manual, se evalua el orden natural: J1, J2, J3, ..." << endl;
        return 1;
    }

    string nombreArchivo = argv[1];

    Instancia instancia = leerInstancia(nombreArchivo);

    vector<int> ordenTrabajos = leerOrdenDesdeArgumentos(argc, argv, instancia.trabajos);

    if (!validarOrden(ordenTrabajos, instancia.trabajos)) {
        cerr << "Error: el orden de trabajos no es una permutacion valida." << endl;
        cerr << "Debe contener todos los trabajos exactamente una vez." << endl;
        cerr << "Ejemplo para 5 trabajos: " << argv[0] << " archivo.txt 3 1 4 2 5" << endl;
        return 1;
    }

    mostrarResumenInstancia(nombreArchivo, instancia);
    mostrarMatriz(instancia);

    cout << "\nOrden de trabajos evaluado: ";
    mostrarOrden(ordenTrabajos);
    cout << endl;

    vector<vector<long long>> tablaFinalizacion =
        generarTablaFinalizacion(instancia, ordenTrabajos);

    mostrarTablaFinalizacion(tablaFinalizacion, ordenTrabajos);

    long long makespan = calcularMakespan(instancia, ordenTrabajos);

    cout << "\nMakespan obtenido: " << makespan << endl;

    mostrarDatosGantt(instancia, ordenTrabajos, tablaFinalizacion);

    string base = obtenerNombreBase(nombreArchivo);

    string archivoFinalizacion = "salida_" + base + "_tabla_finalizacion.csv";
    string archivoGantt = "salida_" + base + "_datos_gantt.csv";
    string archivoResumen = "salida_" + base + "_resumen.txt";

    exportarTablaFinalizacionCSV(archivoFinalizacion, tablaFinalizacion, ordenTrabajos);
    exportarDatosGanttCSV(archivoGantt, instancia, ordenTrabajos, tablaFinalizacion);
    exportarResumenTXT(archivoResumen, nombreArchivo, instancia, ordenTrabajos, makespan);

    cout << "\nArchivos generados:" << endl;
    cout << " - " << archivoFinalizacion << endl;
    cout << " - " << archivoGantt << endl;
    cout << " - " << archivoResumen << endl;

    cout << "\nNota: este resultado evalua el orden indicado. El algoritmo ABC luego usara estas funciones" << endl;
    cout << "para probar muchos ordenes distintos y buscar el menor makespan." << endl;

    return 0;
}
