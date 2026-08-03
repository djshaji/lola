#include <iostream>
#include <vector>
#include <string>
#include "lola.h"

int main(int argc, char **argv) {
    const std::string configPath = argc > 1 ? argv[1] : "src/Ratatouille.lv2/Ratatouille.json";
    const int sampleRate = 48000;
    const int bufferSize = 1024;

    std::cout << "Loading Ratatouille from " << configPath << "\n";
    Plugin plugin(configPath, 0, sampleRate, bufferSize);

    if (plugin.dlhandle == nullptr || plugin.descriptor == nullptr || plugin.handle == nullptr) {
        std::cerr << "Plugin failed to load or instantiate" << std::endl;
        return 1;
    }

    std::vector<float> input(bufferSize, 0.0f);
    std::vector<float> output(bufferSize, 0.0f);

    const int result = plugin.process(input.data(), output.data(), bufferSize);
    std::cout << "process() returned " << result << std::endl;
    std::cout << "first output sample: " << output[0] << std::endl;
    return result == 0 ? 0 : 2;
}
