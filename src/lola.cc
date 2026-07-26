#include "lola.h"

using json = nlohmann::json;
Plugin::Plugin(std::string config, int index, int sampleRate) {
    // parse json from config file
    std::ifstream file(config);
    if (!file.is_open()) {
        LOGE("Failed to open config file: %s\n", config.c_str());
        return;
    }
    json j;
    file >> j;
    j = j[index];
    this->config = j ;
    this->sofile = j["binary"];
    this->index = index;
    this->sampleRate = sampleRate;

    dlhandle = dlopen(sofile.c_str(), RTLD_NOW);
    if (!dlhandle) {
        LOGE("Failed to load shared library: %s\n", sofile.c_str());
        return;
    }

    void *sym = dlsym(dlhandle, "lv2_descriptor");
    if (!sym) {
        LOGE("Failed to find lv2_descriptor in shared library: %s\n", sofile.c_str());
        dlclose(dlhandle);
        return;
    }

    descriptor = ((LV2_Descriptor *(*)(int))sym)(index);
    
    if (!descriptor) {
        LOGE("Failed to find lv2_descriptor in shared library: %s\n", sofile.c_str());
        dlclose(dlhandle);
        return;
    }

    handle = (LV2_Handle *) descriptor->instantiate(descriptor, sampleRate, nullptr, nullptr);
    if (!handle) {
        LOGE("Failed to instantiate LV2 plugin: %s\n", sofile.c_str());
        dlclose(dlhandle);
        return;
    }

    LOGD("Successfully loaded LV2 plugin: %s\n", sofile.c_str());
    loadControls();
}

bool Plugin::loadControls() {
    IN ;
    LOGD("Loading controls for plugin: %s\n", sofile.c_str());
    // parse json from config file
    json j = this->config;
    int index = 0 ;

    for (auto &port : j["ports"]) {
        Control control;

        std::string type = port["type"];
        LOGD("Loading port: %s, type: %s\n", port["name"].get<std::string>().c_str(), type.c_str());
        if (type == "control") {
            control.type = PortType::lCONTROL;
        } else if (type == "audio") {
            control.type = PortType::lAUDIO;
        } else if (type == "midi") {
            control.type = PortType::lMIDI;
        } else if (type == "file") {
            control.type = PortType::lFILE;
        } else if (type == "toggle") {
            control.type = PortType::lTOGGLE;
        } else if (type == "trigger") {
            control.type = PortType::lTRIGGER;
        } else {
            LOGE("Unknown port type: %s\n", type.c_str());
            continue;
        }

        control.index = index++;

        if (control.type != PortType::lCONTROL) {
            if (control.type == PortType::lAUDIO) {
                if (port ["direction"] == "input") {
                    if (audioIn == nullptr) {
                        audioIn = new float[MAX_SAMPLES];
                    } else if (audioIn2 == nullptr) {
                        audioIn2 = new float[MAX_SAMPLES];
                    }
                } else if (port["direction"] == "output") {
                    if (audioOut == nullptr) {
                        audioOut = new float[MAX_SAMPLES];
                    } else if (audioOut2 == nullptr) {
                        audioOut2 = new float[MAX_SAMPLES];
                    }
                }
            }

            continue;
        }

        control.min = port["minimum"];
        control.max = port["maximum"];
        control.def = port["default"];
        control.value = port["default"];
        control.name = port["name"];

        controls.push_back(control);
    }
 
    OUT
    return true;
}