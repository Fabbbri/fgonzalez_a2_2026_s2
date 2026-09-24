#pragma once
#include <iosfwd>
#include <string>

namespace interferogram {
struct Config {
    std::string a = "data/raw/P09-R2/S1A_SLC_20250110T112318_179298_IW2_VV_462858.SAFE/measurement/s1a-iw2-slc-vv-179298-20250110T112318-462858.tiff";
    std::string b = "data/raw/P09-R2/S1A_SLC_20250122T112317_179298_IW2_VV_464635.SAFE/measurement/s1a-iw2-slc-vv-179298-20250122T112317-464635.tiff";
    int x = 0;
    int y = 0;
    int width = 512;
    int height = 512;
    bool help = false;
};
Config parseConfig(int argc, char* argv[]);
void printHelp(std::ostream& out);
} // namespace interferogram
