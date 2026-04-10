#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// ============================================================
// sorting.cpp
// Programa principal para medir tiempo y memoria de:
//   - merge sort
//   - quick sort
//   - std::sort
//
// Flujo:
//   1) Recorre todos los .txt de la carpeta de entrada
//   2) Lee cada arreglo
//   3) Ejecuta los 3 algoritmos sobre COPIAS del mismo arreglo
//   4) Guarda cada arreglo ordenado en la carpeta de salida
//   5) Guarda todas las mediciones en un solo CSV

// ============================================================

namespace fs = std::filesystem;
using Reloj = std::chrono::high_resolution_clock;

// ------------------------------------------------------------
// Declaraciones de las funciones de ordenamiento
// ------------------------------------------------------------
std::vector<int> mergeSortArray(std::vector<int>& arr);
std::vector<int> quickSortArray(std::vector<int>& arr);
std::vector<int> sortArray(std::vector<int>& arr);

struct MetadatosArchivo {
    std::size_t n = 0;
    std::string disposicion;
    std::string cantidad_arreglos;
    std::string distinguidor;
};

struct Medicion {
    std::string archivo_entrada;
    std::string algoritmo;
    std::size_t n = 0;
    std::string disposicion;
    std::string cantidad_arreglos;
    std::string distinguidor;
    long long tiempo_us = 0;
    double tiempo_ms = 0.0;
    long long memoria_kb = -1;
    std::string archivo_salida;
};

// ------------------------------------------------------------
// Rutas
// ------------------------------------------------------------
static const fs::path BASE_DIR = fs::path("data");
static const fs::path CARPETA_ENTRADA = BASE_DIR / "array_input";
static const fs::path CARPETA_SALIDA = BASE_DIR / "array_output";
static const fs::path CARPETA_MEDICIONES = BASE_DIR / "measurements";
static const fs::path ARCHIVO_CSV = CARPETA_MEDICIONES / "sorting_measurements.csv";

// ------------------------------------------------------------
// Extrae metadatos desde el nombre del archivo
// ------------------------------------------------------------
static MetadatosArchivo extraer_metadatos_del_nombre(const fs::path& ruta) {
    MetadatosArchivo meta;
    const std::string base = ruta.stem().string();

    const std::regex patron(R"(^([0-9]+)_([^_]+)_(D[0-9]+)_([a-zA-Z])$)");
    std::smatch coincidencia;
    if (std::regex_match(base, coincidencia, patron)) {
        meta.n = static_cast<std::size_t>(std::stoull(coincidencia[1].str()));
        meta.disposicion = coincidencia[2].str();
        meta.cantidad_arreglos = coincidencia[3].str();
        meta.distinguidor = coincidencia[4].str();
    }

    return meta;
}

// ------------------------------------------------------------
// Lee el arreglo del .txt
// ------------------------------------------------------------
static std::vector<int> leer_arreglo_txt(const fs::path& ruta) {
    std::ifstream entrada(ruta);
    if (!entrada) {
        throw std::runtime_error("No se pudo abrir el archivo de entrada: " + ruta.string());
    }

    std::vector<int> arreglo;
    long long valor = 0;
    while (entrada >> valor) {
        arreglo.push_back(static_cast<int>(valor));
    }

    return arreglo;
}

// ------------------------------------------------------------
// Escribe arreglo ordenado en .txt
// ------------------------------------------------------------
static void escribir_arreglo_txt(const fs::path& ruta, const std::vector<int>& arreglo) {
    std::ofstream salida(ruta, std::ios::trunc);
    if (!salida) {
        throw std::runtime_error("No se pudo abrir el archivo de salida: " + ruta.string());
    }

    salida << arreglo.size() << '\n';
    for (std::size_t i = 0; i < arreglo.size(); ++i) {
        salida << arreglo[i];
        if (i + 1 < arreglo.size()) {
            salida << ' ';
        }
    }
    salida << '\n';
}

// ------------------------------------------------------------
// Medición de memoria
// ------------------------------------------------------------
#ifdef _WIN32
static long long memoria_actual_kb() {
    PROCESS_MEMORY_COUNTERS_EX info{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info), sizeof(info))) {
        return static_cast<long long>(info.WorkingSetSize / 1024ULL);
    }
    return -1;
}
#elif defined(__linux__)
#include <sys/resource.h>
static long long memoria_actual_kb() {
    struct rusage uso {};
    if (getrusage(RUSAGE_SELF, &uso) == 0) {
        return static_cast<long long>(uso.ru_maxrss);
    }
    return -1;
}
#else
static long long memoria_actual_kb() {
    return -1;
}
#endif

// ------------------------------------------------------------
// Ejecuta y mide un algoritmo sobre una copia del arreglo
// ------------------------------------------------------------
static Medicion medir_un_algoritmo(
    const std::string& nombre_archivo,
    const MetadatosArchivo& meta,
    const std::vector<int>& entrada,
    const std::string& nombre_algoritmo,
    std::vector<int> (*funcion_ordenamiento)(std::vector<int>&),
    const fs::path& carpeta_salida
) {
    std::vector<int> trabajo = entrada;

    const long long memoria_antes = memoria_actual_kb();
    const auto inicio = Reloj::now();
    trabajo = funcion_ordenamiento(trabajo);
    const auto fin = Reloj::now();
    const long long memoria_despues = memoria_actual_kb();

    Medicion m;
    m.archivo_entrada = nombre_archivo;
    m.algoritmo = nombre_algoritmo;
    m.n = entrada.size();
    m.disposicion = meta.disposicion;
    m.cantidad_arreglos = meta.cantidad_arreglos;
    m.distinguidor = meta.distinguidor;
    m.tiempo_us = std::chrono::duration_cast<std::chrono::microseconds>(fin - inicio).count();
    m.tiempo_ms = std::chrono::duration<double, std::milli>(fin - inicio).count();

    if (memoria_antes >= 0 && memoria_despues >= 0) {
        m.memoria_kb = std::max(0LL, memoria_despues - memoria_antes);
    }

    const std::string base = fs::path(nombre_archivo).stem().string();
    const fs::path archivo_salida = carpeta_salida / (base + "_" + nombre_algoritmo + ".txt");
    escribir_arreglo_txt(archivo_salida, trabajo);
    m.archivo_salida = archivo_salida.string();

    return m;
}

static void escribir_encabezado_csv(std::ofstream& salida) {
    salida << "archivo_entrada,algoritmo,n,disposicion,cantidad_arreglos,distinguidor,tiempo_us,tiempo_ms,memoria_kb,archivo_salida\n";
}

static void escribir_fila_csv(std::ofstream& salida, const Medicion& m) {
    salida << m.archivo_entrada << ','
           << m.algoritmo << ','
           << m.n << ','
           << m.disposicion << ','
           << m.cantidad_arreglos << ','
           << m.distinguidor << ','
           << m.tiempo_us << ','
           << m.tiempo_ms << ','
           << m.memoria_kb << ','
           << m.archivo_salida << '\n';
}

static std::vector<fs::path> listar_archivos_txt(const fs::path& carpeta) {
    if (!fs::exists(carpeta)) {
        throw std::runtime_error("No existe la carpeta de entrada: " + carpeta.string());
    }

    std::vector<fs::path> archivos;
    for (const auto& entrada : fs::directory_iterator(carpeta)) {
        if (!entrada.is_regular_file()) continue;
        if (entrada.path().extension() != ".txt") continue;
        archivos.push_back(entrada.path());
    }

    std::sort(archivos.begin(), archivos.end(), [](const fs::path& a, const fs::path& b) {
        const auto meta_a = extraer_metadatos_del_nombre(a);
        const auto meta_b = extraer_metadatos_del_nombre(b);
        if (meta_a.n != meta_b.n) return meta_a.n < meta_b.n;
        return a.filename().string() < b.filename().string();
    });

    return archivos;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "[DEBUG] Programa iniciado" << std::endl;

        // Si quieres cambiar rutas sin editar el código, puedes pasar:
        //   --input_dir X --output_dir Y --measurements_dir Z --csv W
        fs::path carpeta_entrada = CARPETA_ENTRADA;
        fs::path carpeta_salida = CARPETA_SALIDA;
        fs::path carpeta_mediciones = CARPETA_MEDICIONES;
        fs::path archivo_csv = ARCHIVO_CSV;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto siguiente = [&](const std::string& nombre) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("Falta un valor después de " + nombre);
                }
                return argv[++i];
            };

            if (arg == "--input_dir") {
                carpeta_entrada = siguiente(arg);
            } else if (arg == "--output_dir") {
                carpeta_salida = siguiente(arg);
            } else if (arg == "--measurements_dir") {
                carpeta_mediciones = siguiente(arg);
            } else if (arg == "--csv") {
                archivo_csv = siguiente(arg);
            } else {
                throw std::runtime_error("Argumento desconocido: " + arg);
            }
        }

        std::cout << "[DEBUG] Carpeta de entrada: " << carpeta_entrada.string() << std::endl;
        std::cout << "[DEBUG] Carpeta de salida: " << carpeta_salida.string() << std::endl;
        std::cout << "[DEBUG] Carpeta de mediciones: " << carpeta_mediciones.string() << std::endl;
        std::cout << "[DEBUG] Archivo CSV: " << archivo_csv.string() << std::endl;

        std::cout << "[DEBUG] Creando/verificando carpetas..." << std::endl;
        fs::create_directories(carpeta_salida);
        fs::create_directories(carpeta_mediciones);

        std::ofstream csv(archivo_csv, std::ios::out | std::ios::trunc);
        if (!csv) {
            throw std::runtime_error("No se pudo abrir el archivo CSV: " + archivo_csv.string());
        }
        escribir_encabezado_csv(csv);
        csv.flush();
        std::cout << "[DEBUG] Encabezado CSV escrito" << std::endl;

        std::cout << "[DEBUG] Leyendo archivos .txt..." << std::endl;
        const std::vector<fs::path> archivos = listar_archivos_txt(carpeta_entrada);
        std::cout << "[DEBUG] Archivos encontrados: " << archivos.size() << std::endl;
        for (const auto& p : archivos) {
            std::cout << "[DEBUG] Orden de procesamiento: " << p.filename().string() << std::endl;
        }

        if (archivos.empty()) {
            std::cerr << "No se encontraron archivos .txt en: " << carpeta_entrada.string() << '\n';
            return 0;
        }

        std::size_t archivos_procesados = 0;
        std::size_t filas_escritas = 0;

        for (const auto& ruta_archivo : archivos) {
            std::cout << "\n[DEBUG] -----------------------------" << std::endl;
            const std::string nombre_archivo = ruta_archivo.filename().string();
            std::cout << "[DEBUG] Procesando: " << nombre_archivo << std::endl;

            const MetadatosArchivo meta = extraer_metadatos_del_nombre(ruta_archivo);
            const std::vector<int> arreglo = leer_arreglo_txt(ruta_archivo);
            std::cout << "[DEBUG] Tamaño del arreglo leído: " << arreglo.size() << std::endl;

            if (arreglo.empty()) {
                std::cerr << "Se omite archivo vacío: " << nombre_archivo << '\n';
                continue;
            }

            if (meta.n != 0 && meta.n != arreglo.size()) {
                std::cerr << "Aviso: el tamaño del nombre no coincide con el contenido en "
                          << nombre_archivo << " (nombre=" << meta.n
                          << ", contenido=" << arreglo.size() << ").\n";
            }

            ++archivos_procesados;

            std::cout << "[DEBUG] Ejecutando merge sort..." << std::endl;
            const Medicion m1 = medir_un_algoritmo(nombre_archivo, meta, arreglo, "merge", mergeSortArray, carpeta_salida);
            std::cout << "[DEBUG] Merge terminado en " << m1.tiempo_us << " us (" << m1.tiempo_ms << " ms)" << std::endl;

            std::cout << "[DEBUG] Ejecutando quick sort..." << std::endl;
            const Medicion m2 = medir_un_algoritmo(nombre_archivo, meta, arreglo, "quick", quickSortArray, carpeta_salida);
            std::cout << "[DEBUG] Quick terminado en " << m2.tiempo_us << " us (" << m2.tiempo_ms << " ms)" << std::endl;

            std::cout << "[DEBUG] Ejecutando std::sort..." << std::endl;
            const Medicion m3 = medir_un_algoritmo(nombre_archivo, meta, arreglo, "stdsort", sortArray, carpeta_salida);
            std::cout << "[DEBUG] std::sort terminado en " << m3.tiempo_us << " us (" << m3.tiempo_ms << " ms)" << std::endl;

            escribir_fila_csv(csv, m1);
            escribir_fila_csv(csv, m2);
            escribir_fila_csv(csv, m3);
            csv.flush();
            std::cout << "[DEBUG] CSV actualizado" << std::endl;

            filas_escritas += 3;
            std::cout << "[DEBUG] Archivo ordenado guardado en: " << carpeta_salida.string() << std::endl;
        }

        std::cout << "\n[DEBUG] Archivos procesados: " << archivos_procesados << '\n';
        std::cout << "[DEBUG] Filas escritas en CSV: " << filas_escritas << '\n';
        std::cout << "[DEBUG] Resultados guardados en: " << archivo_csv.string() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
