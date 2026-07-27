#ifndef __LOLA_H__
#define __LOLA_H__

#include <lv2.h>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <cstring>
#include <fstream>
#include "json.hpp"
#include "log.h"

#define MAX_SAMPLES 4096 * 2

typedef enum {
    lCONTROL,
    lAUDIO,
    lMIDI,
    lFILE,
    lTOGGLE,
    lTRIGGER,
} PortType;

class Control {
public:
    PortType type;
    int index;
    float min;
    float max;
    float def;
    float value;
    std::string name;
};

class Plugin {
public:
    std::string uri;
    int index;
    int sampleRate;

    std::string name;
    std::string description;
    std::string author;
    std::string sofile;
    std::string bundle;

    nlohmann::json config;

    void * dlhandle;
    LV2_Descriptor *descriptor;
    LV2_Handle *handle;

    LV2_Handle *(*instantiate)(const LV2_Descriptor *descriptor, double sample_rate, const char *bundle_path, const LV2_Feature *const *features);
     connect_port;
    void * activate;
    void * run;
    void * deactivate;

    std::vector<Control> controls;

    int audioIn = -1;
    int audioIn2 = -1;

    int audioOut = -1;
    int audioOut2 = -1;

    // atom port for sending file names
    int atomPort = -1;
    void * atomBuffer = nullptr;

    Plugin(std::string config, int index, int sampleRate);
    // void printInfo();
    bool loadControls();
};

#endif // __LOLA_H__