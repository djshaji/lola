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

    handle = descriptor->instantiate(descriptor, sampleRate, nullptr, nullptr);
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
                if (port["direction"] == "input") {
                    if (audioIn == -1) {
                        audioIn = control.index;
                    } else if (audioIn2 == -1) {
                        audioIn2 = control.index;
                    }
                } else if (port["direction"] == "output") {
                    if (audioOut == -1) {
                        audioOut = control.index;
                    } else if (audioOut2 == -1) {
                        audioOut2 = control.index;
                    }
                }
            } else if (control.type == PortType::lFILE) {
                atomPort = control.index;
                atomBuffer = malloc(MAX_SAMPLES * sizeof(float));
            } else {
                LOGE("Unsupported port type: %s\n", type.c_str());
            }

            continue;
        }

        control.min = port["minimum"];
        control.max = port["maximum"];
        control.def = port["default"];
        control.value = new float(control.def);
        control.name = port["name"];

        descriptor->connect_port(handle, control.index, control.value);

        controls.push_back(control);
    }
 
    descriptor->activate(handle);
    LOGD("Plugin loaded successfully\n");
    return true;
}

int Plugin::process (float *in, float *out, int nframes) {
    IN
    if (audioIn == -1 || audioOut == -1) {
        LOGE("Audio ports not connected\n");
        return -1;
    }

    descriptor->connect_port(handle, audioIn, in);
    descriptor->connect_port(handle, audioOut, out);

    if (audioIn2 != -1 && audioOut2 != -1) {
        descriptor->connect_port(handle, audioIn2, in + nframes);
        descriptor->connect_port(handle, audioOut2, out + nframes);
    }

    descriptor->run(handle, nframes);
    return 0;
}