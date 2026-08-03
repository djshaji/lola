#ifndef __LOLA_H__
#define __LOLA_H__

#include <lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/options/options.h>
#include <lv2/patch/patch.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>
#include <lv2/uri-map/uri-map.h>
#include <lv2/worker/worker.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
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
    lUNKNOWN
} PortType;

class Control {
public:
    PortType type;
    int index;
    float min;
    float max;
    float def;
    float * value;
    std::string name;
    bool input;
};

class Plugin {
public:
    std::string uri;
    int index;
    int sampleRate;
    int hostBufferSize = 0;

    bool enabled = true;

    std::string name;
    std::string description;
    std::string author;
    std::string sofile;
    std::string bundle;

    nlohmann::json config;

    void * dlhandle;
    LV2_Descriptor *descriptor;
    LV2_Handle handle;

    std::vector<Control> controls;
    std::vector<Control> monitorControls;
    std::vector<LV2_Feature> featureEntries;
    std::queue<std::pair<uint32_t, std::vector<uint8_t>>> workerQueue;
    std::queue<std::string> patchQueue;
    std::vector<const LV2_Feature*> featurePointers;
    std::vector<void*> ownedFeatureData;
    std::vector<std::string> featureUris;

    int audioIn = -1;
    int audioIn2 = -1;

    int audioOut = -1;
    int audioOut2 = -1;

    // atom port for sending file names
    int atomPort = -1;
    void * atomBuffer = nullptr;
    std::unordered_map<int, void*> atomPortBuffers;

    Plugin(std::string config, int index, int sampleRate, int bufferSize = 0);
    ~Plugin();
    // void printInfo();
    bool loadControls();
    int process(float *in, float *out, int nframes);
    uint32_t mapUri(const std::string &uri);
    void drainWorkerQueue();
    void saveState();
    void restoreState();
    void queuePatch(const std::string &message);
    void drainPatchQueue();

private:
    struct UriMapData {
        std::unordered_map<std::string, uint32_t> uriToUrid;
        std::unordered_map<uint32_t, std::string> uridToUri;
        uint32_t nextUrid = 1;
    };

    struct BufferSizeData {
        uint32_t minBlockLength = 1;
        uint32_t maxBlockLength = 0;
        uint32_t nominalBlockLength = 0;
    };

    struct OptionsData {
        uint32_t minBlockLength = 1;
        uint32_t maxBlockLength = 0;
        uint32_t nominalBlockLength = 0;
        uint32_t atomIntType = 0;
        uint32_t minKey = 0;
        uint32_t maxKey = 0;
        uint32_t nominalKey = 0;
        LV2_Options_Option entries[4]{};
    };

    struct WorkerData {
        Plugin *plugin = nullptr;
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> pending;
    };

    struct StateEntry {
        uint32_t key = 0;
        uint32_t type = 0;
        uint32_t flags = 0;
        std::vector<uint8_t> data;
    };

    static uint32_t uriToIdCallback(LV2_URI_Map_Callback_Data callback_data,
                                    const char *map,
                                    const char *uri);
    static LV2_URID uridMapCallback(LV2_URID_Map_Handle handle,
                                    const char *uri);
    static LV2_Worker_Status scheduleWorkCallback(LV2_Worker_Schedule_Handle handle,
                                                 uint32_t size,
                                                 const void *data);
    void buildFeatureList();

    UriMapData *uriMapData = nullptr;
    LV2_URID_Map uridMapFeature{};
    LV2_URI_Map_Feature uriMapFeature{};
    BufferSizeData bufferSizeData{};
    OptionsData optionsData{};
    LV2_Worker_Schedule workerScheduleFeature{};
    WorkerData workerData{};
    std::unordered_map<uint32_t, StateEntry> stateMap;
};

#endif // __LOLA_H__