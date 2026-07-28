/*  Scaffold to test LV2 Library
 *
 *  This file is part of the LV2 Loader project.
 *
 *  Copyright (C) 2024  DJ Shaji
 *  MIT License

*/

#include <iostream>
#include <dlfcn.h>
#include <jack/jack.h>
#include <signal.h>

#include "lola.h"

// Global client pointer to allow clean shutdown on Ctrl+C
jack_client_t *client = NULL;
jack_port_t *input_port = NULL;
jack_port_t *output_port = NULL;

int process_callback(jack_nframes_t nframes, void *arg) {
    // Get buffers for input and output ports
    jack_default_audio_sample_t *in = (jack_default_audio_sample_t *)jack_port_get_buffer(input_port, nframes);
    jack_default_audio_sample_t *out = (jack_default_audio_sample_t *)jack_port_get_buffer(output_port, nframes);

    // Cast the argument to a Plugin pointer
    Plugin *plugin = static_cast<Plugin *>(arg);
    plugin->process(in, out, nframes);
    // Minimal processing: Copy input directly to output (Passthrough)
    // for (jack_nframes_t i = 0; i < nframes; i++) {
    //     out[i] = in[i];
    // }

    return 0; // Return 0 to indicate success
}

/**
 * Shutdown callback. Called if the JACK server shuts down or crashes.
 */
void jack_shutdown_callback(void *arg) {
    fprintf(stderr, "JACK server shut down unexpectedly.\n");
    exit(1);
}

/**
 * Signal handler for Ctrl+C to ensure clean exit.
 */
void signal_handler(int sig) {
    if (client != NULL) {
        printf("\nShutting down JACK client...\n");
        jack_deactivate(client);
        jack_client_close(client);
    }
    exit(0);
}

int main(int argc, char **argv) {
    jack_options_t options = JackNullOption;
    jack_status_t status;

    // Set up signal handler for clean termination
    signal(SIGINT, signal_handler);

    // 1. Open the client connection to the server
    client = jack_client_open("minimal_c_client", options, &status);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server.\n");
        }
        return 1;
    }

    Plugin * plugin = new Plugin("dyson_compress-swh.lv2/plugin.json", 0, jack_get_sample_rate(client));

    // 2. Register the real-time processing callback
    jack_set_process_callback(client, process_callback, plugin);

    // Register the shutdown callback
    jack_on_shutdown(client, jack_shutdown_callback, NULL);

    // Display current system parameters
    printf("JACK client opened. Sample rate: %u Hz\n", jack_get_sample_rate(client));
    printf("Current buffer size: %u frames\n", jack_get_buffer_size(client));

    // 3. Register audio ports (1 Input, 1 Output)
    input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if ((input_port == NULL) || (output_port == NULL)) {
        fprintf(stderr, "Could not register ports.\n");
        jack_client_close(client);
        return 1;
    }

    // 4. Activate the client (tells the server to start calling process_callback)
    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate client.\n");
        jack_client_close(client);
        return 1;
    }

    printf("Client is active. Press Ctrl+C to exit.\n");

    // Main execution thread idles here while the real-time thread runs
    while (1) {
        sleep(1);
    }

    return 0;
}