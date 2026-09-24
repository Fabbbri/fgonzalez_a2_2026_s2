#include <cstdint>
#include <fstream>
#include <iostream>
#include <Eigen/Dense>

using std::cout;

struct Sample {
    Eigen::VectorXd x;
    Eigen::VectorXd y;
};

// ----------------------------------------------
// LECTOR DE BINARIO BIG-ENDIAN -> LITTLE ENDIAN
// ----------------------------------------------
uint32_t read_uint32(std::ifstream& file) {
    uint8_t bytes[4];

    file.read(reinterpret_cast<char*>(bytes), 4);

    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8)  |
            static_cast<uint32_t>(bytes[3]);
}

// ----------------------------------------------
// CARGAR UNA IMAGEN
// ----------------------------------------------
Eigen::VectorXd load_image(std::ifstream& file, int index) {

    constexpr int pixels_per_image = 28 * 28;
    constexpr int header_size = 16;

    // Ir al inicio de la imagen solicitada
    file.seekg(
        header_size + index * pixels_per_image,
        std::ios::beg
    );

    uint8_t pixels[pixels_per_image];

    file.read(
        reinterpret_cast<char*>(pixels),
        pixels_per_image
    );

    // Crear vector Eigen de 784 elementos
    Eigen::VectorXd image(pixels_per_image);

    // Normalizar [0,255] -> [0,1]
    for (int i = 0; i < pixels_per_image; ++i) {
        image(i) = static_cast<double>(pixels[i]) / 255.0;
    }

    return image;
}

// ----------------------------------------------
// CARGAR UNA ETIQUETA
// ----------------------------------------------
int load_label(std::ifstream& file, int index) {

    constexpr int header_size = 8;

    file.seekg(
        header_size + index,
        std::ios::beg
    );

    uint8_t label;

    file.read(
        reinterpret_cast<char*>(&label),
        1
    );

    return static_cast<int>(label);
}

// ----------------------------------------------
// CARGAR IMAGEN+ETIQUETA EN STRUCT SAMPLE
// ----------------------------------------------
Sample load_sample(
    std::ifstream& image_file,
    std::ifstream& label_file,
    int index
) {
    Sample sample;

    sample.x = load_image(image_file, index);

    int label = load_label(label_file, index);

    // ------------------------------------------
    // Transformar etiqueta a one-hot vector
    // ------------------------------------------

    sample.y = Eigen::VectorXd::Zero(10);
    sample.y(label) = 1.0;

    return sample;
}

// ----------------------------------------------
// MAIN
// ----------------------------------------------
int main() {

    // ----------------------------------------------
    // Imagen a acceder
    // ----------------------------------------------
    int index = 5000;
    cout << "Index = " << index << '\n';

    // ----------------------
    // Abrir archivos
    // ----------------------

    std::ifstream file(
        "../data/mnist/train-images-idx3-ubyte",
        std::ios::binary
    );

    if (!file) {
        std::cerr << "No se pudo abrir la imagen\n";
        return 1;
    }

    std::ifstream labels_file(
        "../data/mnist/train-labels-idx1-ubyte",
        std::ios::binary
    );

    if (!labels_file) {
        std::cerr << "No se pudo abrir la etiqueta\n";
        return 1;
    }

    // ----------------------
    // Leer headers
    // ----------------------

    uint32_t magic = read_uint32(file);
    uint32_t images = read_uint32(file);
    uint32_t rows = read_uint32(file);
    uint32_t columns = read_uint32(file);

    uint32_t label_magic = read_uint32(labels_file);
    uint32_t label_count = read_uint32(labels_file);

    // ----------------------
    // Mostrar información
    // ----------------------

    std::cout << "Magic: " << magic << '\n';
    std::cout << "Images: " << images << '\n';
    std::cout << "Rows: " << rows << '\n';
    std::cout << "Columns: " << columns << '\n';

    std::cout << "Label magic: " << label_magic << '\n';
    std::cout << "Labels: " << label_count << '\n';

    // ----------------------
    // Cargar primera muestra
    // ----------------------

    Sample sample = load_sample(file, labels_file, index);
    Eigen::VectorXd x = sample.x;
    Eigen::VectorXd y = sample.y;

    // ----------------------
    // Mostrar resultado
    // ----------------------

    std::cout << "\nTamaño de X: " << x.size() << '\n';
    std::cout << "Min: " << x.minCoeff() << '\n';
    std::cout << "Max: " << x.maxCoeff() << '\n';

    std::cout << "Y: " << '\n';
    for (int i = 0; i < 10; ++i) {
        std::cout << y(i) << ' ';
    }
    std::cout << '\n';

    return 0;
}