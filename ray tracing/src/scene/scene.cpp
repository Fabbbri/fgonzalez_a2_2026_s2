/**
 * @file scene.cpp
 * Construcción determinista de una escena de esferas sobre un suelo esférico.
 * La colocación valida separación geométrica antes de insertar cada objeto.
 * La generación de la escena precede al intervalo medido de los modelos.
 */
#include "scene/scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace rt {
// Devuelve una escena por valor; toda la construcción ocurre en un único hilo.
Scene default_scene() {
    // Inicializa geometría vacía y parámetros de iluminación y calidad predeterminados.
    Scene scene;
    scene.background = {0.48, 0.66, 0.95};

    // Composición inspirada en "A Final Render" de Ray Tracing in One Weekend:
    // https://raytracing.github.io/books/RayTracingInOneWeekend.html
    // Materiales mates, metálicos y vidrio; el cálculo sigue siendo secuencial.
    // Centro profundo: la superficie superior de una esfera grande aproxima un suelo plano.
    const Vec3 ground_center{0, -1001, -6};
    constexpr double ground_radius = 1000;
    // Inserta el soporte como elemento cero; la separación posterior excluye este índice.
    scene.spheres.push_back({ground_center, ground_radius, {0.42, 0.44, 0.46}});

    // La altura hace que cada esfera toque la superficie del suelo curvo.
    // La separación adicional evita tanto interpenetraciones como contactos
    // demasiado ajustados. El suelo no se compara: es el soporte de todas.
    constexpr double min_gap = 0.16;
    constexpr double glass_gap = 1.2;
    // Calcula la altura del centro de un objeto tangente al suelo esférico.
    const auto ground_position = [&](double x, double z, double radius) {
        // La tangencia exterior exige distancia entre centros R_suelo+r_objeto.
        const double distance = ground_radius + radius;
        // Desplazamiento horizontal respecto al centro del suelo en el eje z.
        const double dz = z - ground_center.z;
        // Despeja y de x²+(y-y_suelo)²+dz²=(R+r)² usando la raíz superior.
        return Vec3{x, ground_center.y + std::sqrt(distance*distance - x*x - dz*dz), z};
    };
    // Evalúa el menor margen libre frente a objetos existentes, sin modificar la escena.
    const auto clearance = [&](Vec3 center, double radius, MaterialType type) {
        // Inicializa el mínimo; con ningún objeto adicional permanece en infinito.
        double nearest_margin = std::numeric_limits<double>::infinity();
        // Recorre esferas decorativas y principales; omite el suelo de índice cero.
        for (std::size_t i = 1; i < scene.spheres.size(); ++i) {
            const Sphere& other = scene.spheres[i];
            // Vector entre centros para obtener la separación euclidiana.
            const Vec3 delta = center - other.center;
            // Dos cristales necesitan más espacio; para el resto rige min_gap.
            const double required = type == MaterialType::glass &&
                                    other.material.type == MaterialType::glass
                                  ? glass_gap : min_gap;
            // Distancia entre superficies menos separación requerida; negativo implica rechazo.
            const double margin = std::sqrt(dot(delta, delta)) - radius - other.radius - required;
            // Conserva la restricción más ajustada entre todos los vecinos.
            nearest_margin = std::min(nearest_margin, margin);
        }
        return nearest_margin;
    };
    // Operación de inserción condicionada: construye posición, valida y agrega.
    // La captura por referencia permite consultar y actualizar scene.
    const auto add_sphere = [&](double x, double z, double radius, Vec3 color,
                                Material material = {}) {
        const Vec3 center = ground_position(x, z, radius);
        // Rechaza interpenetración o separación insuficiente; false indica no inserción.
        if (clearance(center, radius, material.type) < 0.0) return false;
        scene.spheres.push_back({center, radius, color, material});
        return true;
    };

    // Tres esferas principales: difusa, dieléctrica y metálica.
    add_sphere(-3.2, -11.2, 1.0, {0.68, 0.30, 0.16});
    add_sphere(0.0, -11.5, 1.15, {1, 1, 1}, {MaterialType::glass, 0.0, 1.5});
    add_sphere(3.2, -11.2, 1.5, {0.88, 0.72, 0.42}, {MaterialType::metal, 0.03});

    // La semilla fija permite repetir exactamente
    // la escena en cada ejecución y comparar modelos con la misma carga.
    std::mt19937 random(2026);
    // Normaliza la salida entera de mt19937 mediante 2^32 para producir valores [0,1).
    const auto sample = [&] { return static_cast<double>(random()) / 4294967296.0; };
    // Categorías discretas que seleccionan intervalos de radio.
    enum class Size { small, medium, large };
    std::vector<Size> sizes;
    sizes.insert(sizes.end(), 49, Size::small);   // ~65 %
    sizes.insert(sizes.end(), 19, Size::medium);  // ~25 %
    sizes.insert(sizes.end(), 7, Size::large);    // ~10 %
    // Permuta categorías con el generador fijo para evitar un orden espacial por tamaños.
    std::shuffle(sizes.begin(), sizes.end(), random);

    // HSV permite elegir tonos vivos sin que los canales RGB produzcan grises por azar.
    // Transforma tono, saturación y valor a canales RGB por sectores del círculo cromático.
    const auto hsv_to_rgb = [](double hue, double saturation, double value) -> Vec3 {
        // Escala el tono normalizado a los seis sectores de color.
        const double h = hue * 6.0;
        // Croma: diferencia entre canales máximo y mínimo.
        const double chroma = value * saturation;
        // Componente intermedia que varía linealmente dentro del sector.
        const double secondary = chroma * (1.0 - std::abs(std::fmod(h, 2.0) - 1.0));
        // Nivel común que se suma para recuperar el brillo especificado.
        const double minimum = value - chroma;
        // Parte entera del tono escalado; los retornos permutan las componentes RGB.
        const int sector = static_cast<int>(h);
        if (sector == 0) return {value, secondary + minimum, minimum};
        if (sector == 1) return {secondary + minimum, value, minimum};
        if (sector == 2) return {minimum, value, secondary + minimum};
        if (sector == 3) return {minimum, secondary + minimum, value};
        if (sector == 4) return {secondary + minimum, minimum, value};
        return {value, minimum, secondary + minimum};
    };

    // Acota candidatos por esfera; el algoritmo siempre termina aunque no haya espacio.
    constexpr int max_attempts = 100;
    // Cuenta las posiciones aceptadas para variar el tono de la siguiente decoración.
    std::size_t color_index = 0;
    // Genera hasta 75 decoraciones; este bucle construye datos, no paraleliza el render.
    for (const Size size : sizes) {
        // Intervalo inicial de esferas pequeñas, sustituido por switch para otras categorías.
        double min_radius = 0.12, max_radius = 0.25;
        switch (size) {
        case Size::small:  break;
        case Size::medium: min_radius = 0.25; max_radius = 0.45; break;
        case Size::large:  min_radius = 0.45; max_radius = 0.70; break;
        }
        // Interpolación uniforme dentro del intervalo de radio seleccionado.
        const double radius = min_radius + (max_radius - min_radius) * sample();
        // Elegir el material antes permite validar también el espacio entre cristales.
        // Muestra para elegir material: vidrio sobre 0.88 y metal entre 0.65 y 0.88.
        const double choice = sample();
        Material material;
        if (choice > 0.88) material = {MaterialType::glass, 0.0, 1.5};
        else if (choice > 0.65) material = {MaterialType::metal, 0.04 + 0.18 * sample()};
        // Posición candidata ganadora, inicialmente en cero sin que esto implique aceptación.
        Vec3 best{};
        // Valor centinela negativo: ningún candidato válido ha sido seleccionado aún.
        double best_score = -1.0;
        // Muestreo de candidatos sin filas. Elegir el que más espacio libre
        // tiene reparte las esferas por la escena sin perder la irregularidad.
        // Las decorativas grandes van más atrás para conservar visibles las
        // protagonistas; las pequeñas pueden ocupar también el primer plano.
        // Semiancho de la zona de colocación, menor para decoraciones grandes.
        const double x_limit = size == Size::large ? 5.0 : 5.5;
        // Límite frontal dependiente del tamaño para conservar visibles las esferas principales.
        const double front_z = size == Size::large ? -6.5
                             : size == Size::medium ? -4.0 : -2.5;
        // Límite posterior común de la zona de colocación.
        constexpr double back_z = -12.5;
        // Explora candidatos y conserva el de mayor separación mínima disponible.
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            // Muestra uniforme de posición horizontal en [-x_limit,x_limit).
            const double x = (2.0 * sample() - 1.0) * x_limit;
            // Interpolación de la profundidad entre límites frontal y posterior.
            const double z = front_z + (back_z - front_z) * sample();
            // Eleva el centro hasta la posición de tangencia con el suelo.
            const Vec3 candidate = ground_position(x, z, radius);
            // Margen geométrico frente a vecinos para el material previamente seleccionado.
            const double gap = clearance(candidate, radius, material.type);
            if (gap < 0.0) continue;
            // Favorecer algo de margen en los extremos del encuadre.
            // Distancia mínima a los bordes de la zona para evitar agrupación en sus extremos.
            const double edge = std::min(x_limit - std::abs(x),
                                         std::min(front_z - z, z - back_z));
            // Criterio maximin: limita la calidad por vecinos y por bordes.
            const double score = std::min(gap, edge);
            // Sustituye la solución solo cuando mejora el criterio de separación.
            if (score > best_score) {
                best = candidate;
                best_score = score;
            }
        }
        if (best_score < 0.0) continue; // Evita bucles sin fin si se llena la zona.

        // El paso áureo reparte los tonos por toda la paleta; un leve azar
        // evita un patrón rígido. Saturación y brillo altos evitan colores apagados.
        // La semilla fija hace reproducible tanto la posición como el color.
        constexpr double golden_step = 0.6180339887498949;
        const double hue = std::fmod(0.17 + color_index * golden_step
                                      + 0.08 * sample(), 1.0);
        const double saturation = 0.72 + 0.23 * sample();
        const double value = 0.82 + 0.16 * sample();
        const Vec3 color = hsv_to_rgb(hue, saturation, value);
        // En difusos eleva RGB al cuadrado para compensar el brillo tras gamma aproximada.
        const Vec3 albedo = material.type == MaterialType::diffuse ? color * color : color;
        // Inserta el mejor candidato con las mismas verificaciones de tangencia y separación.
        add_sphere(best.x, best.z, radius, albedo, material);
        ++color_index;
    }

    return scene;
}
} // namespace rt
