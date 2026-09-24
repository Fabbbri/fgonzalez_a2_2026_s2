#include "interferogram/slc_reader.hpp"
#include "interferogram/config.hpp"
#include <gdal_priv.h>
#include <cpl_vsi.h>
#include <cpl_error.h>
#include <iostream>
#include <stdexcept>

using namespace interferogram;
namespace {
/* Lanza std::runtime_error con message cuando la condición ok no se cumple. */
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
/*
 * Ejecuta f y exige que lance una excepción derivada de std::exception.
 * Falla la prueba si la operación acepta la entrada que debía rechazar.
 */
template<class F> void rejects(F f) {
    bool rejected=false;
    try { f(); } catch (const std::exception&) { rejected=true; }
    require(rejected, "Entrada inválida aceptada");
}
/*
 * Crea en path un TIFF con las dimensiones, bandas y tipo recibidos.
 * Para CInt16 escribe una ventana 2×2 en (7, 11); requiere espacio para ella.
 * Lanza std::runtime_error si falla el controlador, la creación o la escritura.
 */
void fixture(const char* path, int width, int height, int bands, GDALDataType type) {
    // No almacenar bloques sin escribir permite representar un burst grande con pocos datos.
    char sparse[]="SPARSE_OK=YES";
    char* options[]{sparse, nullptr};
    auto* driver=GetGDALDriverManager()->GetDriverByName("GTiff");
    require(driver != nullptr, "Driver GTiff ausente");
    auto* dataset=driver->Create(path,width,height,bands,type,options);
    require(dataset != nullptr, "Error creando fixture");
    if (type==GDT_CInt16) {
        // Pares real/imaginario con signos y extremos del rango Int16.
        double values[]{2,3, -4,5, 32767,-32768, 0,-9};
        const auto status=dataset->GetRasterBand(1)->RasterIO(
            GF_Write,7,11,2,2,values,2,2,GDT_CFloat64,0,0,nullptr);
        GDALClose(dataset);
        require(status==CE_None, "Error escribiendo fixture");
    } else GDALClose(dataset);
}
}
/*
 * Comprueba conversión compleja, offsets, rechazos de formato y ventanas reales A/B.
 * Requiere los TIFF locales indicados en Config y el controlador GTiff de GDAL.
 * Devuelve 0 al pasar las pruebas y 1 ante una excepción.
 */
int main() {
    GDALAllRegister();
    // /vsimem almacena el TIFF temporal en memoria mediante GDAL.
    const char* path="/vsimem/test_slc_complex.tif";
    try {
        fixture(path,slcWidth,slcHeight,1,GDT_CInt16);
        const auto values=readSlcWindow(path,7,11,2,2);
        const Complex expected[]{{2,3},{-4,5},{32767,-32768},{0,-9}};
        require(values.size()==4, "Tamaño sintético incorrecto");
        for (std::size_t i=0;i<4;++i)
            require(values[i].real==expected[i].real && values[i].imag==expected[i].imag,
                    "Se perdieron o intercambiaron componentes real/imag");
        // Rechazar coordenadas negativas, límites excedidos y dimensiones nulas.
        rejects([&]{readSlcWindow(path,-1,0,1,1);});
        rejects([&]{readSlcWindow(path,slcWidth-1,0,2,1);});
        rejects([&]{readSlcWindow(path,0,0,0,1);});
        VSIUnlink(path);
        // Recrear el archivo para comprobar tipo no complejo, bandas y dimensiones.
        fixture(path,slcWidth,slcHeight,1,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        fixture(path,slcWidth,slcHeight,2,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        fixture(path,10,10,1,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        // Suprimir el mensaje esperado de GDAL al intentar abrir un archivo inexistente.
        CPLPushErrorHandler(CPLQuietErrorHandler);
        rejects([]{readSlcWindow("/vsimem/nonexistent_slc.tif",0,0,1,1);});
        CPLPopErrorHandler();
        // Integración con las adquisiciones reales: conservar componentes imaginarias.
        Config config;
        for (const auto& source : {config.a,config.b}) {
            const auto real=readSlcWindow(source,12000,750,16,16);
            require(real.size()==256,"Ventana real no tiene 256 muestras");
            bool imaginary=false;
            for (const auto& value:real) imaginary |= value.imag!=0;
            require(imaginary, "Ventana real sin componentes imaginarias no nulas");
        }
        std::cout << "PASS: conversión compleja, offsets, errores y ventanas reales A/B de 16x16\n";
    } catch (const std::exception& e) {
        VSIUnlink(path);
        std::cerr << e.what() << '\n'; return 1;
    }
}
