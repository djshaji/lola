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

    std::vector<Control> controls;

    float * audioIn = nullptr;
    float * audioIn2 = nullptr;

    float * audioOut = nullptr;
    float * audioOut2 = nullptr;
    
    Plugin(std::string config, int index, int sampleRate);
    // void printInfo();
    bool loadControls();
};

#endif // __LOLA_H__