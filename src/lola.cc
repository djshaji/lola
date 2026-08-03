#include "lola.h"

#include <cstdlib>
#include <filesystem>
#include <lv2/atom/util.h>

using json = nlohmann::json;

Plugin::~Plugin() {
    for (auto &control : controls) {
        if (control.value != nullptr) {
            delete control.value;
            control.value = nullptr;
        }
    }
    controls.clear();

    for (auto &control : monitorControls) {
        if (control.value != nullptr) {
            delete control.value;
            control.value = nullptr;
        }
    }
    monitorControls.clear();

    if (handle != nullptr && descriptor != nullptr && descriptor->cleanup != nullptr) {
        descriptor->cleanup(handle);
    }

    if (atomBuffer != nullptr) {
        free(atomBuffer);
        atomBuffer = nullptr;
    }

    for (auto &[portIndex, buffer] : atomPortBuffers) {
        if (buffer != nullptr) {
            free(buffer);
        }
    }
    atomPortBuffers.clear();

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

    featureUris.emplace_back(LV2_BUF_SIZE__boundedBlockLength);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), nullptr});

    workerData.plugin = this;
    workerScheduleFeature.handle = &workerData;
    workerScheduleFeature.schedule_work = &Plugin::scheduleWorkCallback;

    featureUris.emplace_back(LV2_WORKER__schedule);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), &workerScheduleFeature});

    if (uriMapData == nullptr) {
        uriMapData = new UriMapData();
    }

    optionsData.minBlockLength = bufferSizeData.minBlockLength;
    optionsData.maxBlockLength = bufferSizeData.maxBlockLength;
    optionsData.nominalBlockLength = bufferSizeData.nominalBlockLength;
    optionsData.atomIntType = mapUri(LV2_ATOM__Int);
    optionsData.minKey = mapUri(LV2_BUF_SIZE__minBlockLength);
    optionsData.maxKey = mapUri(LV2_BUF_SIZE__maxBlockLength);
    optionsData.nominalKey = mapUri(LV2_BUF_SIZE__nominalBlockLength);

    optionsData.entries[0] = LV2_Options_Option{
        LV2_OPTIONS_INSTANCE,
        0,
        optionsData.minKey,
        sizeof(optionsData.minBlockLength),
        optionsData.atomIntType,
        &optionsData.minBlockLength
    };

    optionsData.entries[1] = LV2_Options_Option{
        LV2_OPTIONS_INSTANCE,
        0,
        optionsData.maxKey,
        sizeof(optionsData.maxBlockLength),
        optionsData.atomIntType,
        &optionsData.maxBlockLength
    };

    optionsData.entries[2] = LV2_Options_Option{
        LV2_OPTIONS_INSTANCE,
        0,
        optionsData.nominalKey,
        sizeof(optionsData.nominalBlockLength),
        optionsData.atomIntType,
        &optionsData.nominalBlockLength
    };

    optionsData.entries[3] = LV2_Options_Option{};

    featureUris.emplace_back(LV2_OPTIONS__options);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), optionsData.entries});

    uridMapFeature.handle = uriMapData;
    uridMapFeature.map = &Plugin::uridMapCallback;

    featureUris.emplace_back(LV2_URID__map);
    featureEntries.push_back(LV2_Feature{featureUris.back().c_str(), &uridMapFeature});

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

    const std::filesystem::path configPath(config);
    std::filesystem::path bundlePath = this->bundle.empty()
        ? configPath.parent_path()
        : std::filesystem::path(this->bundle);
    if (bundlePath.empty()) {
        bundlePath = std::filesystem::current_path();
    }
    this->bundle = bundlePath.string();

    std::filesystem::path libraryPath(this->sofile);
    if (libraryPath.is_relative()) {
        libraryPath = bundlePath / libraryPath;
    }
    this->sofile = libraryPath.string();

    dlhandle = dlopen(this->sofile.c_str(), RTLD_NOW);
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

    const char *bundle_path = this->bundle.empty() ? nullptr : this->bundle.c_str();
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

void Plugin::resetAtomPortBuffer(void *buffer) {
    if (buffer == nullptr) {
        return;
    }

    std::memset(buffer, 0, MAX_SAMPLES);
    auto *seq = static_cast<LV2_Atom_Sequence *>(buffer);
    seq->atom.type = mapUri(LV2_ATOM__Sequence);
    seq->atom.size = sizeof(LV2_Atom_Sequence_Body);
    seq->body.unit = 0;
    seq->body.pad = 0;
}

LV2_URID Plugin::uridMapCallback(LV2_URID_Map_Handle handle,
                                 const char *uri) {
    if (handle == nullptr || uri == nullptr) {
        return 0;
    }

    auto *data = static_cast<UriMapData *>(handle);
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
        const std::string name = port.value("name", "");
        const std::string symbol = port.value("symbol", "");
        control.input = (port["direction"] == "input");

        LOGD("Loading port: %s, type: %s\n", port["name"].get<std::string>().c_str(), type.c_str());
        const bool isAtomPort = type == "atom" || (type == "other" && (name == "CONTROL" || name == "NOTIFY" || symbol == "CONTROL" || symbol == "NOTIFY"));
        if (type == "control") {
            control.type = PortType::lCONTROL;
        } else if (type == "audio") {
            control.type = PortType::lAUDIO;
        } else if (type == "midi") {
            control.type = PortType::lMIDI;
        } else if (type == "file") {
            control.type = PortType::lFILE;
        } else if (isAtomPort) {
            control.type = PortType::lUNKNOWN;
            control.index = index++;
            control.name = port["name"];
            control.min = 0.0f;
            control.max = 1.0f;
            control.def = 0.0f;
            control.value = nullptr;

            void *buffer = calloc(1, MAX_SAMPLES);
            if (buffer != nullptr) {
                resetAtomPortBuffer(buffer);
                atomPortBuffers[control.index] = buffer;
                descriptor->connect_port(handle, control.index, buffer);
                if (control.input && atomPort == -1) {
                    atomPort = control.index;
                }
            } else {
                LOGE("Failed to allocate atom buffer for port: %s\n", control.name.c_str());
            }
            continue;
        } else if (type == "toggle") {
            control.type = PortType::lTOGGLE;
        } else if (type == "trigger") {
            control.type = PortType::lTRIGGER;
        } else {
            LOGE("Unknown port type: %s\n", type.c_str());
            control.type = PortType::lUNKNOWN;
            control.index = index++;
            control.name = port["name"];
            control.min = 0.0f;
            control.max = 1.0f;
            control.def = 0.0f;
            control.value = new float(control.def);
            descriptor->connect_port(handle, control.index, control.value);
            monitorControls.push_back(control);
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

        control.min = port.value("minimum", 0.0f);
        control.max = port.value("maximum", 1.0f);
        control.def = port.value("default", control.min);
        control.value = new float(control.def);
        control.name = port.value("name", "");

        descriptor->connect_port(handle, control.index, control.value);

        if (control.input) {
            controls.push_back(control);
        } else {
            // Output controls must still be connected or plugins may write through null port pointers.
            monitorControls.push_back(control);
            LOGD("Connected output control port: %s\n", control.name.c_str());
        }
    }

    if (j.contains("pathProperties") && j["pathProperties"].is_array()) {
        for (const auto &property : j["pathProperties"]) {
            if (atomPort == -1) {
                LOGE("No input atom port available for file property: %s\n",
                     property.value("uri", "").c_str());
                break;
            }

            Control fileControl;
            fileControl.type = PortType::lFILE;
            fileControl.index = atomPort;
            fileControl.min = 0.0f;
            fileControl.max = 0.0f;
            fileControl.def = 0.0f;
            fileControl.value = nullptr;
            fileControl.input = true;
            fileControl.plugin = this;
            fileControl.name = property.value("label", property.value("uri", "File"));
            fileControl.propertyUri = property.value("uri", "");
            fileControl.fileTypes = property.value("fileTypes", "");
            controls.push_back(fileControl);
        }
    }
 
    LOGD("Loaded %zu controls for plugin: %s\n", controls.size(), sofile.c_str());
    restoreState();
    LOGD("Restored state for plugin: %s\n", sofile.c_str());
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

    for (auto &[portIndex, buffer] : atomPortBuffers) {
        if (portIndex != atomPort) {
            resetAtomPortBuffer(buffer);
        }
    }

    if (audioIn2 != -1 && audioOut2 != -1) {
        // main.cc currently exposes a single input/output JACK port.
        // Mirror mono buffers to secondary plugin audio ports instead of indexing past the JACK buffer.
        descriptor->connect_port(handle, audioIn2, in);
        descriptor->connect_port(handle, audioOut2, out);
    }

    descriptor->run(handle, frames);

    auto atomInput = atomPortBuffers.find(atomPort);
    if (atomInput != atomPortBuffers.end()) {
        resetAtomPortBuffer(atomInput->second);
    }
    return 0;
}

bool Plugin::sendFileNameToAtomPort(int port, const std::string &propertyUri, const std::string &filename) {
    if (port < 0 || atomPortBuffers.find(port) == atomPortBuffers.end()) {
        LOGE("Invalid atom port index: %d\n", port);
        return false;
    }

    void *buffer = atomPortBuffers[port];
    if (buffer == nullptr) {
        LOGE("Atom buffer not allocated for port: %d\n", port);
        return false;
    }

    if (propertyUri.empty()) {
        LOGE("Empty property URI for atom port: %d\n", port);
        return false;
    }

    if (filename.empty()) {
        LOGE("Empty filename for atom port: %d\n", port);
        return false;
    }

    resetAtomPortBuffer(buffer);

    auto *seq = static_cast<LV2_Atom_Sequence *>(buffer);
    const uint32_t objectType = mapUri(LV2_ATOM__Object);
    const uint32_t patchSetType = mapUri(LV2_PATCH__Set);
    const uint32_t patchPropertyKey = mapUri(LV2_PATCH__property);
    const uint32_t patchValueKey = mapUri(LV2_PATCH__value);
    const uint32_t uridType = mapUri(LV2_ATOM__URID);
    const uint32_t pathType = mapUri(LV2_ATOM__Path);
    const uint32_t propertyUrid = mapUri(propertyUri);

    const uint32_t uridValueSize = sizeof(uint32_t);
    const uint32_t pathValueSize = static_cast<uint32_t>(filename.size() + 1);
    const uint32_t propertyChunkSize = static_cast<uint32_t>(sizeof(LV2_Atom_Property_Body)) + lv2_atom_pad_size(uridValueSize);
    const uint32_t valueChunkSize = static_cast<uint32_t>(sizeof(LV2_Atom_Property_Body)) + lv2_atom_pad_size(pathValueSize);
    const uint32_t objectPayloadSize = static_cast<uint32_t>(sizeof(LV2_Atom_Object_Body)) + propertyChunkSize + valueChunkSize;
    const uint32_t eventSize = static_cast<uint32_t>(sizeof(LV2_Atom_Event)) + lv2_atom_pad_size(objectPayloadSize);
    const uint32_t required = static_cast<uint32_t>(sizeof(LV2_Atom_Sequence)) + eventSize;
    if (required > MAX_SAMPLES) {
        LOGE("Filename too large for atom buffer on port %d: %zu bytes\n", port, filename.size());
        return false;
    }

    auto *event = reinterpret_cast<LV2_Atom_Event *>(reinterpret_cast<uint8_t *>(seq) + sizeof(LV2_Atom_Sequence));
    event->time.frames = 0;
    event->body.type = objectType;
    event->body.size = objectPayloadSize;

    auto *object = reinterpret_cast<LV2_Atom_Object_Body *>(event + 1);
    object->id = 0;
    object->otype = patchSetType;

    uint8_t *cursor = reinterpret_cast<uint8_t *>(object + 1);
    auto appendProperty = [](uint8_t *dest, uint32_t key, uint32_t valueType, const void *value, uint32_t valueSize) {
        auto *property = reinterpret_cast<LV2_Atom_Property_Body *>(dest);
        property->key = key;
        property->context = 0;
        property->value.type = valueType;
        property->value.size = valueSize;

        uint8_t *body = reinterpret_cast<uint8_t *>(property) + sizeof(LV2_Atom_Property_Body);
        std::memcpy(body, value, valueSize);

        const uint32_t paddedSize = lv2_atom_pad_size(valueSize);
        if (paddedSize > valueSize) {
            std::memset(body + valueSize, 0, paddedSize - valueSize);
        }

        return body + paddedSize;
    };

    cursor = appendProperty(cursor, patchPropertyKey, uridType, &propertyUrid, uridValueSize);
    cursor = appendProperty(cursor, patchValueKey, pathType, filename.c_str(), pathValueSize);

    seq->atom.size += eventSize;
    LOGD("Queued patch:Set on atom port %d for %s -> %s\n", port, propertyUri.c_str(), filename.c_str());
    return true;
}