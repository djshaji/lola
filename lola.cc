#include "lola.h"

Plugin::Plugin(std::string sofile, std::string bundle, int index, int sampleRate) {
    this->sofile = sofile;
    this->bundle = bundle;
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

    handle = (LV2_Handle *) descriptor->instantiate(descriptor, sampleRate, bundle.c_str(), nullptr);
    if (!handle) {
        LOGE("Failed to instantiate LV2 plugin: %s\n", sofile.c_str());
        dlclose(dlhandle);
        return;
    }


}

