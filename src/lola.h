#ifndef __LOLA_H__
#define __LOLA_H__

#include <lv2.h>
#include <string>
#include <vector>
#include <dlfcn.h>

#include "log.h"

typedef enum {
    CONTROL,
    AUDIO,
    MIDI,
    FILE,
    TOGGLE,
    TRIGGER,
} PortType;

class Control {
    PortType type;
    int index;
    float min;
    float max;
    float def;
    float value;
    std::string name;
};

class Plugin {
    std::string uri;
    int index;
    int sampleRate;

    std::string name;
    std::string description;
    std::string author;
    std::string sofile;
    std::string bundle;

    void * dlhandle;
    LV2_Descriptor *descriptor;
    LV2_Handle *handle;

    std::vector<Control> controls;
    
    Plugin(std::string sofile, std::string bundle, int index, int sampleRate);
};

#endif // __LOLA_H__