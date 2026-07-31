#include "lola.h"

#include <cstdlib>

using json = nlohmann::json;

Plugin::~Plugin() {
    for (auto &control : controls) {
        if (control.value != nullptr) {
            delete control.value;
            control.value = nullptr;
        }
    }
    controls.clear();

    if (handle != nullptr && descriptor != nullptr && descriptor->cleanup != nullptr) {
        descriptor->cleanup(handle);
    }

    if (atomBuffer != nullptr) {
        free(atomBuffer);
        atomBuffer = nullptr;
    }

    if (uriMapData != nullptr) {
        delete uriMapData;
        uriMapData = nullptr;
    }

    for (void *ptr : ownedFeatureData) {
        if (ptr != nullptr) {
            free(ptr);
        }
    }
    ownedFeatureData.clear();

    if (dlhandle != nullptr) {
        dlclose(dlhandle);
        dlhandle = nullptr;
    }
}

void Plugin::buildFeatureList() {
    featureEntries.clear();
    featurePointers.clear();
    ownedFeatureData.clear();
    featureUris.clear();

    static const char *kHardRTCapableURI = "http://lv2plug.in/ns/lv2core#hardRTCapable";
    featureUris.emplace_back(kHardRTCapableURI);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), nullptr});

    bufferSizeData.minBlockLength = 1;
    bufferSizeData.maxBlockLength = hostBufferSize > 0 ? static_cast<uint32_t>(hostBufferSize) : 0;
    bufferSizeData.nominalBlockLength = hostBufferSize > 0 ? static_cast<uint32_t>(hostBufferSize) : 0;

    featureUris.emplace_back(LV2_BUF_SIZE_URI);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), &bufferSizeData});

    workerData.plugin = this;
    workerScheduleFeature.handle = &workerData;
    workerScheduleFeature.schedule_work = &Plugin::scheduleWorkCallback;

    featureUris.emplace_back(LV2_WORKER__schedule);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), &workerScheduleFeature});

    if (uriMapData == nullptr) {
        uriMapData = new UriMapData();
    }

    uriMapFeature.callback_data = uriMapData;
    uriMapFeature.uri_to_id = &Plugin::uriToIdCallback;

    featureUris.emplace_back(LV2_URI_MAP_URI);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), &uriMapFeature});

    featurePointers.reserve(featureEntries.size() + 1);
    for (auto &entry : featureEntries) {
        featurePointers.push_back(&entry);
    }
    featurePointers.push_back(nullptr);
}

Plugin::Plugin(std::string config, int index, int sampleRate, int bufferSize) {
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
    this->bundle = j.value("bundle", "");
    this->index = index;
    this->sampleRate = sampleRate;
    this->hostBufferSize = bufferSize;

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

    buildFeatureList();

    const char *bundle_path = bundle.empty() ? nullptr : bundle.c_str();
    handle = descriptor->instantiate(descriptor, sampleRate, bundle_path,
                                     featurePointers.empty() ? nullptr : featurePointers.data());
    if (!handle) {
        LOGE("Failed to instantiate LV2 plugin: %s\n", sofile.c_str());
        dlclose(dlhandle);
        return;
    }

    LOGD("Successfully loaded LV2 plugin: %s\n", sofile.c_str());

    loadControls();
}

uint32_t Plugin::mapUri(const std::string &uri) {
    if (uriMapData == nullptr) {
        uriMapData = new UriMapData();
    }

    auto it = uriMapData->uriToUrid.find(uri);
    if (it != uriMapData->uriToUrid.end()) {
        return it->second;
    }

    const uint32_t id = uriMapData->nextUrid++;
    uriMapData->uriToUrid[uri] = id;
    uriMapData->uridToUri[id] = uri;
    return id;
}

LV2_Worker_Status Plugin::scheduleWorkCallback(LV2_Worker_Schedule_Handle handle,
                                                uint32_t size,
                                                const void *data) {
    if (handle == nullptr) {
        return LV2_WORKER_ERR_UNKNOWN;
    }

    auto *worker = static_cast<WorkerData *>(handle);
    if (worker->plugin == nullptr) {
        return LV2_WORKER_ERR_UNKNOWN;
    }

    std::vector<uint8_t> payload;
    if (size > 0 && data != nullptr) {
        payload.assign(static_cast<const uint8_t *>(data),
                       static_cast<const uint8_t *>(data) + size);
    }

    worker->plugin->workerQueue.emplace(size, std::move(payload));
    return LV2_WORKER_SUCCESS;
}

uint32_t Plugin::uriToIdCallback(LV2_URI_Map_Callback_Data callback_data,
                                 const char *map,
                                 const char *uri) {
    (void)map;
    if (callback_data == nullptr || uri == nullptr) {
        return 0;
    }

    auto *data = static_cast<UriMapData *>(callback_data);
    const std::string uriValue(uri);

    auto it = data->uriToUrid.find(uriValue);
    if (it != data->uriToUrid.end()) {
        return it->second;
    }

    const uint32_t id = data->nextUrid++;
    data->uriToUrid[uriValue] = id;
    data->uridToUri[id] = uriValue;
    return id;
}

void Plugin::saveState() {
    stateMap.clear();

    for (const auto &control : controls) {
        if (control.value == nullptr) {
            continue;
        }

        StateEntry entry;
        entry.key = mapUri("http://lola.local/state/control/" + control.name);
        entry.type = mapUri("http://lv2plug.in/ns/ext/atom#Float");
        entry.flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

        const float value = *control.value;
        entry.data.resize(sizeof(float));
        std::memcpy(entry.data.data(), &value, sizeof(float));
        stateMap[entry.key] = std::move(entry);
    }
}

void Plugin::restoreState() {
    for (auto &control : controls) {
        if (control.value == nullptr) {
            continue;
        }

        const uint32_t key = mapUri("http://lola.local/state/control/" + control.name);
        auto it = stateMap.find(key);
        if (it == stateMap.end()) {
            continue;
        }

        if (it->second.data.size() == sizeof(float)) {
            float value = 0.0f;
            std::memcpy(&value, it->second.data.data(), sizeof(float));
            *control.value = value;
        }
    }
}

void Plugin::queuePatch(const std::string &message) {
    patchQueue.push(message);
}

void Plugin::drainPatchQueue() {
    while (!patchQueue.empty()) {
        const std::string patch = std::move(patchQueue.front());
        patchQueue.pop();

        if (patch.empty()) {
            continue;
        }

        LOGD("Drained patch message: %s\n", patch.c_str());
    }
}

void Plugin::drainWorkerQueue() {
    while (!workerQueue.empty()) {
        auto item = std::move(workerQueue.front());
        workerQueue.pop();

        if (item.second.empty()) {
            continue;
        }

        const std::string message(item.second.begin(), item.second.end());
        LOGD("Drained worker message: %s\n", message.c_str());
    }
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
                const size_t bufferFrames = hostBufferSize > 0 ? static_cast<size_t>(hostBufferSize) : MAX_SAMPLES;
                atomBuffer = malloc(bufferFrames * sizeof(float));
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
 
    restoreState();
    descriptor->activate(handle);
    LOGD("Plugin loaded successfully\n");
    return true;
}

int Plugin::process (float *in, float *out, int nframes) {
    if (! enabled) {
        // bypass processing, copy input to output
        memcpy(out, in, nframes * sizeof(float));
        return 0;
    }
    
    if (audioIn == -1 || audioOut == -1) {
        LOGE("Audio ports not connected\n");
        return -1;
    }

    const int frames = (hostBufferSize > 0 && nframes > hostBufferSize) ? hostBufferSize : nframes;

    descriptor->connect_port(handle, audioIn, in);
    descriptor->connect_port(handle, audioOut, out);

    if (audioIn2 != -1 && audioOut2 != -1) {
        descriptor->connect_port(handle, audioIn2, in + frames);
        descriptor->connect_port(handle, audioOut2, out + frames);
    }

    descriptor->run(handle, frames);
    return 0;
}