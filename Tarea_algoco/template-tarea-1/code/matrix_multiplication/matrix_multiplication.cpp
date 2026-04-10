#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// ============================================================
// matrix_multiplication.cpp
//
// Programa principal para:
//   - leer pares de matrices desde data/matrix_input/
//   - ejecutar multiplicación naive y Strassen
//   - guardar matrices resultado en data/matrix_output/
//   - escribir mediciones en data/measurements/
//
// Formato esperado de archivo:
//   16_densa_D0_a_1.txt   -> matriz izquierda
//   16_densa_D0_a_2.txt   -> matriz derecha
//
// Contenido del archivo:
//   n x n números enteros separados por espacios y/o saltos de línea.
//   El tamaño se obtiene desde el nombre del archivo.
//
// IMPORTANTE:
//   - Este archivo usa implementaciones vectoriales de naive y Strassen
//     para que el driver pueda llamarlas directamente.
//   - Si quieres conservar tus archivos originales separados, puedes
//     dejarlos como referencia, pero este driver necesita una firma
//     reutilizable.
// ============================================================

namespace fs = std::filesystem;
using Reloj = std::chrono::high_resolution_clock;
using Matriz = std::vector<std::vector<long long>>;

// ------------------------------------------------------------
// Ajusta aquí tus rutas si tu estructura cambia
// ------------------------------------------------------------
static const fs::path BASE_DIR = fs::path("data");
static const fs::path CARPETA_ENTRADA = BASE_DIR / "matrix_input";
static const fs::path CARPETA_SALIDA = BASE_DIR / "matrix_output";
static const fs::path CARPETA_MEDICIONES = BASE_DIR / "measurements";
static const fs::path ARCHIVO_CSV = CARPETA_MEDICIONES / "matrix_measurements.csv";
// ------------------------------------------------------------
// Metadatos del archivo
// ------------------------------------------------------------
struct MetadatosArchivo {
    std::size_t n = 0;
    std::string disposicion;
    std::string tipo_datos;
    std::string distinguidor;
    std::string lado;
};

struct DimensionesCaso {
    std::size_t filas_a = 0;
    std::size_t cols_a = 0;
    std::size_t filas_b = 0;
    std::size_t cols_b = 0;
};

struct Medicion {
    std::string archivo_entrada;
    std::string algoritmo;
    std::size_t filas_a = 0;
    std::size_t cols_a = 0;
    std::size_t filas_b = 0;
    std::size_t cols_b = 0;
    long long tiempo_us = 0;
    double tiempo_ms = 0.0;
    long long memoria_kb = -1;
    std::string archivo_salida;
};

// ------------------------------------------------------------
// Utilidades de texto
// ------------------------------------------------------------
static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static bool line_is_empty_or_whitespace(const std::string& line) {
    return trim(line).empty();
}

static std::vector<std::string> split_tokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

// ------------------------------------------------------------
// Parseo del nombre del archivo
// Ejemplo:
//   16_densa_D0_a_1.txt
// ------------------------------------------------------------
static MetadatosArchivo extraer_metadatos_del_nombre(const fs::path& ruta) {
    MetadatosArchivo meta;
    const std::string base = ruta.stem().string();

    const std::regex patron(R"(^([0-9]+)_([^_]+)_(D[0-9]+)_([a-zA-Z])_([12])$)");
    std::smatch coincidencia;
    if (std::regex_match(base, coincidencia, patron)) {
        meta.n = static_cast<std::size_t>(std::stoull(coincidencia[1].str()));
        meta.disposicion = coincidencia[2].str();
        meta.tipo_datos = coincidencia[3].str();
        meta.distinguidor = coincidencia[4].str();
        meta.lado = coincidencia[5].str();
    }

    return meta;
}

// ------------------------------------------------------------
// Lectura de matrices
// Se asume una matriz n x n con n tomado del nombre del archivo.
// ------------------------------------------------------------
static Matriz leer_matriz_txt(const fs::path& ruta, std::size_t n) {
    std::ifstream entrada(ruta);
    if (!entrada) {
        throw std::runtime_error("No se pudo abrir el archivo de entrada: " + ruta.string());
    }

    Matriz A(n, std::vector<long long>(n, 0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (!(entrada >> A[i][j])) {
                throw std::runtime_error("No se pudo leer la matriz completa desde: " + ruta.string());
            }
        }
    }

    return A;
}

// ------------------------------------------------------------
// Escritura de matrices
// ------------------------------------------------------------
static void escribir_matriz_txt(const fs::path& ruta, const Matriz& m) {
    std::ofstream salida(ruta, std::ios::trunc);
    if (!salida) {
        throw std::runtime_error("No se pudo abrir el archivo de salida: " + ruta.string());
    }

    salida << m.size() << ' ' << (m.empty() ? 0 : m[0].size()) << '\n';
    for (const auto& fila : m) {
        for (std::size_t j = 0; j < fila.size(); ++j) {
            salida << fila[j];
            if (j + 1 < fila.size()) salida << ' ';
        }
        salida << '\n';
    }
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
// Utilidades matemáticas para Strassen
// ------------------------------------------------------------
static Matriz matriz_ceros(std::size_t filas, std::size_t cols) {
    return Matriz(filas, std::vector<long long>(cols, 0));
}

static Matriz sumar_matrices(const Matriz& A, const Matriz& B) {
    Matriz C = matriz_ceros(A.size(), A[0].size());
    for (std::size_t i = 0; i < A.size(); ++i) {
        for (std::size_t j = 0; j < A[0].size(); ++j) {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
    return C;
}

static Matriz restar_matrices(const Matriz& A, const Matriz& B) {
    Matriz C = matriz_ceros(A.size(), A[0].size());
    for (std::size_t i = 0; i < A.size(); ++i) {
        for (std::size_t j = 0; j < A[0].size(); ++j) {
            C[i][j] = A[i][j] - B[i][j];
        }
    }
    return C;
}

static Matriz submatriz(const Matriz& A, std::size_t fila0, std::size_t col0, std::size_t n) {
    Matriz R = matriz_ceros(n, n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            R[i][j] = A[fila0 + i][col0 + j];
        }
    }
    return R;
}

static std::size_t next_power_of_two(std::size_t x) {
    std::size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

static Matriz recortar_matriz(const Matriz& A, std::size_t n) {
    Matriz R = matriz_ceros(n, n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            R[i][j] = A[i][j];
        }
    }
    return R;
}

// ------------------------------------------------------------
// Multiplicación naive
// ------------------------------------------------------------
static Matriz matrix_multiply_naive(const Matriz& A, const Matriz& B) {
    const std::size_t n = A.size();
    const std::size_t m = B[0].size();
    const std::size_t l = A[0].size();

    Matriz C = matriz_ceros(n, m);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < l; ++k) {
            const long long aik = A[i][k];
            for (std::size_t j = 0; j < m; ++j) {
                C[i][j] += aik * B[k][j];
            }
        }
    }
    return C;
}

// ------------------------------------------------------------
// Strassen recursivo con padding
// ------------------------------------------------------------
static Matriz strassen_rec(const Matriz& A, const Matriz& B) {
    const std::size_t n = A.size();

    if (n <= 32) {
        return matrix_multiply_naive(A, B);
    }

    const std::size_t k = n / 2;
    const Matriz A11 = submatriz(A, 0, 0, k);
    const Matriz A12 = submatriz(A, 0, k, k);
    const Matriz A21 = submatriz(A, k, 0, k);
    const Matriz A22 = submatriz(A, k, k, k);

    const Matriz B11 = submatriz(B, 0, 0, k);
    const Matriz B12 = submatriz(B, 0, k, k);
    const Matriz B21 = submatriz(B, k, 0, k);
    const Matriz B22 = submatriz(B, k, k, k);

    const Matriz M1 = strassen_rec(sumar_matrices(A11, A22), sumar_matrices(B11, B22));
    const Matriz M2 = strassen_rec(sumar_matrices(A21, A22), B11);
    const Matriz M3 = strassen_rec(A11, restar_matrices(B12, B22));
    const Matriz M4 = strassen_rec(A22, restar_matrices(B21, B11));
    const Matriz M5 = strassen_rec(sumar_matrices(A11, A12), B22);
    const Matriz M6 = strassen_rec(restar_matrices(A21, A11), sumar_matrices(B11, B12));
    const Matriz M7 = strassen_rec(restar_matrices(A12, A22), sumar_matrices(B21, B22));

    Matriz C = matriz_ceros(n, n);
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = 0; j < k; ++j) {
            C[i][j] = M1[i][j] + M4[i][j] - M5[i][j] + M7[i][j];
            C[i][j + k] = M3[i][j] + M5[i][j];
            C[i + k][j] = M2[i][j] + M4[i][j];
            C[i + k][j + k] = M1[i][j] - M2[i][j] + M3[i][j] + M6[i][j];
        }
    }
    return C;
}

static Matriz matrix_multiply_strassen(const Matriz& A, const Matriz& B) {
    const std::size_t n = A.size();
    const std::size_t l = A[0].size();
    const std::size_t m = B[0].size();
    const std::size_t s = next_power_of_two(std::max({n, l, m}));

    Matriz Ap = matriz_ceros(s, s);
    Matriz Bp = matriz_ceros(s, s);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < l; ++j) {
            Ap[i][j] = A[i][j];
        }
    }
    for (std::size_t i = 0; i < l; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            Bp[i][j] = B[i][j];
        }
    }

    Matriz Cp = strassen_rec(Ap, Bp);
    Matriz C = matriz_ceros(n, m);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            C[i][j] = Cp[i][j];
        }
    }
    return C;
}

// ------------------------------------------------------------
// Wrappers llamados por el driver
// ------------------------------------------------------------
Matriz MatrixMultiplyArray(const Matriz& a, const Matriz& b) {
    return matrix_multiply_naive(a, b);
}

Matriz StrassenArray(const Matriz& a, const Matriz& b) {
    return matrix_multiply_strassen(a, b);
}

// ------------------------------------------------------------
// Lectura de todos los archivos y agrupación por caso
// ------------------------------------------------------------
struct CasoArchivo {
    fs::path ruta;
    MetadatosArchivo meta;
};

static std::vector<CasoArchivo> listar_archivos_txt(const fs::path& carpeta) {
    if (!fs::exists(carpeta)) {
        throw std::runtime_error("No existe la carpeta de entrada: " + carpeta.string());
    }

    std::vector<CasoArchivo> archivos;
    for (const auto& entrada : fs::directory_iterator(carpeta)) {
        if (!entrada.is_regular_file()) continue;
        if (entrada.path().extension() != ".txt") continue;

        MetadatosArchivo meta = extraer_metadatos_del_nombre(entrada.path());
        if (meta.n == 0 || meta.lado.empty()) {
            std::cerr << "[WARN] Se omite archivo con nombre no reconocido: "
                      << entrada.path().filename().string() << std::endl;
            continue;
        }

        archivos.push_back({entrada.path(), meta});
    }

    std::sort(archivos.begin(), archivos.end(), [](const CasoArchivo& a, const CasoArchivo& b) {
        if (a.meta.n != b.meta.n) return a.meta.n < b.meta.n;
        if (a.meta.disposicion != b.meta.disposicion) return a.meta.disposicion < b.meta.disposicion;
        if (a.meta.tipo_datos != b.meta.tipo_datos) return a.meta.tipo_datos < b.meta.tipo_datos;
        if (a.meta.distinguidor != b.meta.distinguidor) return a.meta.distinguidor < b.meta.distinguidor;
        return a.meta.lado < b.meta.lado;
    });

    return archivos;
}

struct ClaveCaso {
    std::size_t n;
    std::string disposicion;
    std::string tipo_datos;
    std::string distinguidor;

    bool operator<(const ClaveCaso& other) const {
        return std::tie(n, disposicion, tipo_datos, distinguidor) <
               std::tie(other.n, other.disposicion, other.tipo_datos, other.distinguidor);
    }
};

static ClaveCaso clave_de(const MetadatosArchivo& meta) {
    return {meta.n, meta.disposicion, meta.tipo_datos, meta.distinguidor};
}

// ------------------------------------------------------------
// CSV
// ------------------------------------------------------------
static void escribir_encabezado_csv(std::ofstream& salida) {
    salida << "archivo_entrada,algoritmo,filas_a,cols_a,filas_b,cols_b,tiempo_us,tiempo_ms,memoria_kb,archivo_salida\n";
}

static void escribir_fila_csv(std::ofstream& salida, const Medicion& m) {
    salida << m.archivo_entrada << ','
           << m.algoritmo << ','
           << m.filas_a << ','
           << m.cols_a << ','
           << m.filas_b << ','
           << m.cols_b << ','
           << m.tiempo_us << ','
           << m.tiempo_ms << ','
           << m.memoria_kb << ','
           << m.archivo_salida << '\n';
}

// ------------------------------------------------------------
// Medición de un algoritmo
// ------------------------------------------------------------
static Medicion medir_un_algoritmo(
    const std::string& nombre_archivo,
    const DimensionesCaso& dims,
    const Matriz& A,
    const Matriz& B,
    const std::string& nombre_algoritmo,
    Matriz (*funcion)(const Matriz&, const Matriz&),
    const fs::path& carpeta_salida
) {
    const long long memoria_antes = memoria_actual_kb();
    const auto inicio = Reloj::now();
    Matriz C = funcion(A, B);
    const auto fin = Reloj::now();
    const long long memoria_despues = memoria_actual_kb();

    Medicion m;
    m.archivo_entrada = nombre_archivo;
    m.algoritmo = nombre_algoritmo;
    m.filas_a = dims.filas_a;
    m.cols_a = dims.cols_a;
    m.filas_b = dims.filas_b;
    m.cols_b = dims.cols_b;
    m.tiempo_us = std::chrono::duration_cast<std::chrono::microseconds>(fin - inicio).count();
    m.tiempo_ms = std::chrono::duration<double, std::milli>(fin - inicio).count();
    if (memoria_antes >= 0 && memoria_despues >= 0) {
        m.memoria_kb = std::max(0LL, memoria_despues - memoria_antes);
    }

    const std::string base = fs::path(nombre_archivo).stem().string();
    const fs::path archivo_salida = carpeta_salida / (base + "_" + nombre_algoritmo + ".txt");
    escribir_matriz_txt(archivo_salida, C);
    m.archivo_salida = archivo_salida.string();
    return m;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "[DEBUG] Programa de matrices iniciado" << std::endl;

        fs::path carpeta_entrada = CARPETA_ENTRADA;
        fs::path carpeta_salida = CARPETA_SALIDA;
        fs::path carpeta_mediciones = CARPETA_MEDICIONES;
        fs::path archivo_csv = ARCHIVO_CSV;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto siguiente = [&](const std::string& nombre) -> std::string {
                if (i + 1 >= argc) throw std::runtime_error("Falta un valor después de " + nombre);
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

        std::cout << "[DEBUG] Entrada: " << carpeta_entrada.string() << std::endl;
        std::cout << "[DEBUG] Salida: " << carpeta_salida.string() << std::endl;
        std::cout << "[DEBUG] Mediciones: " << carpeta_mediciones.string() << std::endl;
        std::cout << "[DEBUG] CSV: " << archivo_csv.string() << std::endl;

        fs::create_directories(carpeta_salida);
        fs::create_directories(carpeta_mediciones);

        std::ofstream csv(archivo_csv, std::ios::out | std::ios::trunc);
        if (!csv) {
            throw std::runtime_error("No se pudo abrir el archivo CSV: " + archivo_csv.string());
        }
        escribir_encabezado_csv(csv);
        csv.flush();

        const std::vector<CasoArchivo> archivos = listar_archivos_txt(carpeta_entrada);
        std::cout << "[DEBUG] Archivos válidos encontrados: " << archivos.size() << std::endl;
        for (const auto& a : archivos) {
            std::cout << "[DEBUG] " << a.ruta.filename().string() << std::endl;
        }

        if (archivos.empty()) {
            std::cerr << "No se encontraron archivos válidos en: " << carpeta_entrada.string() << '\n';
            return 0;
        }

        // Agrupar por caso (n, disposición, tipo, distinguidor)
        std::map<ClaveCaso, std::pair<fs::path, fs::path>> pares;
        for (const auto& a : archivos) {
            const ClaveCaso key = clave_de(a.meta);
            auto& par = pares[key];
            if (a.meta.lado == "1") {
                par.first = a.ruta;
            } else if (a.meta.lado == "2") {
                par.second = a.ruta;
            }
        }

        std::size_t archivos_procesados = 0;
        std::size_t filas_escritas = 0;

        for (const auto& [key, par] : pares) {
            if (par.first.empty() || par.second.empty()) {
                std::cerr << "[WARN] Caso incompleto para n=" << key.n
                          << " disposicion=" << key.disposicion
                          << " tipo=" << key.tipo_datos
                          << " distinguidor=" << key.distinguidor << std::endl;
                continue;
            }

            const MetadatosArchivo meta_izq = extraer_metadatos_del_nombre(par.first);
            const MetadatosArchivo meta_der = extraer_metadatos_del_nombre(par.second);
            const std::size_t n = key.n;

            std::cout << "\n[DEBUG] Caso: " << n << "_" << key.disposicion << "_" << key.tipo_datos
                      << "_" << key.distinguidor << std::endl;
            std::cout << "[DEBUG] Izquierda: " << par.first.filename().string() << std::endl;
            std::cout << "[DEBUG] Derecha:   " << par.second.filename().string() << std::endl;

            Matriz A = leer_matriz_txt(par.first, n);
            Matriz B = leer_matriz_txt(par.second, n);

            DimensionesCaso dims;
            dims.filas_a = n;
            dims.cols_a = n;
            dims.filas_b = n;
            dims.cols_b = n;

            ++archivos_procesados;

            std::cout << "[DEBUG] Ejecutando naive..." << std::endl;
            const Medicion m1 = medir_un_algoritmo(par.first.filename().string() + " + " + par.second.filename().string(),
                                                   dims, A, B, "naive", MatrixMultiplyArray, carpeta_salida);
            std::cout << "[DEBUG] Naive terminado: " << m1.tiempo_us << " us (" << m1.tiempo_ms << " ms)" << std::endl;

            std::cout << "[DEBUG] Ejecutando strassen..." << std::endl;
            const Medicion m2 = medir_un_algoritmo(par.first.filename().string() + " + " + par.second.filename().string(),
                                                   dims, A, B, "strassen", StrassenArray, carpeta_salida);
            std::cout << "[DEBUG] Strassen terminado: " << m2.tiempo_us << " us (" << m2.tiempo_ms << " ms)" << std::endl;

            escribir_fila_csv(csv, m1);
            escribir_fila_csv(csv, m2);
            csv.flush();
            std::cout << "[DEBUG] CSV actualizado" << std::endl;

            filas_escritas += 2;
        }

        std::cout << "\n[DEBUG] Casos procesados: " << archivos_procesados << std::endl;
        std::cout << "[DEBUG] Filas escritas: " << filas_escritas << std::endl;
        std::cout << "[DEBUG] CSV final: " << archivo_csv.string() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
