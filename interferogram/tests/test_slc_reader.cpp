#include "interferogram/slc_reader.hpp"
#include "interferogram/config.hpp"
#include <gdal_priv.h>
#include <cpl_vsi.h>
#include <cpl_error.h>
#include <iostream>
#include <stdexcept>

using namespace interferogram;
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class F> void rejects(F f) {
    bool rejected=false;
    try { f(); } catch (const std::exception&) { rejected=true; }
    require(rejected, "Entrada inválida aceptada");
}
void fixture(const char* path, int width, int height, int bands, GDALDataType type) {
    char sparse[]="SPARSE_OK=YES";
    char* options[]{sparse, nullptr};
    auto* driver=GetGDALDriverManager()->GetDriverByName("GTiff");
    require(driver != nullptr, "Driver GTiff ausente");
    auto* dataset=driver->Create(path,width,height,bands,type,options);
    require(dataset != nullptr, "Error creando fixture");
    if (type==GDT_CInt16) {
        double values[]{2,3, -4,5, 32767,-32768, 0,-9};
        const auto status=dataset->GetRasterBand(1)->RasterIO(
            GF_Write,7,11,2,2,values,2,2,GDT_CFloat64,0,0,nullptr);
        GDALClose(dataset);
        require(status==CE_None, "Error escribiendo fixture");
    } else GDALClose(dataset);
}
}
int main() {
    GDALAllRegister();
    const char* path="/vsimem/test_slc_complex.tif";
    try {
        fixture(path,slcWidth,slcHeight,1,GDT_CInt16);
        const auto values=readSlcWindow(path,7,11,2,2);
        const Complex expected[]{{2,3},{-4,5},{32767,-32768},{0,-9}};
        require(values.size()==4, "Tamaño sintético incorrecto");
        for (std::size_t i=0;i<4;++i)
            require(values[i].real==expected[i].real && values[i].imag==expected[i].imag,
                    "Se perdieron o intercambiaron componentes real/imag");
        rejects([&]{readSlcWindow(path,-1,0,1,1);});
        rejects([&]{readSlcWindow(path,slcWidth-1,0,2,1);});
        rejects([&]{readSlcWindow(path,0,0,0,1);});
        VSIUnlink(path);
        fixture(path,slcWidth,slcHeight,1,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        fixture(path,slcWidth,slcHeight,2,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        fixture(path,10,10,1,GDT_Int16);
        rejects([&]{readSlcWindow(path,0,0,1,1);});
        VSIUnlink(path);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        rejects([]{readSlcWindow("/vsimem/nonexistent_slc.tif",0,0,1,1);});
        CPLPopErrorHandler();
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
