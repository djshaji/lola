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
#include <gtk/gtk.h>
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

    Plugin * plugin = new Plugin(argv [1], 0, jack_get_sample_rate(client), jack_get_buffer_size(client));

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

    // auto connect ports to system playback and capture ports
    const char **ports;
    ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
    if (ports == NULL) {
        fprintf(stderr, "No physical capture ports available.\n");
    } else {
        if (jack_connect(client, ports[0], jack_port_name(input_port))) {
            fprintf(stderr, "Cannot connect input ports.\n");
        }
        free(ports);
    }

    // 4. Activate the client (tells the server to start calling process_callback)
    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate client.\n");
        jack_client_close(client);
        return 1;
    }

    printf("Client is active. Press Ctrl+C to exit.\n");

    GtkWidget *window;
    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "LV2 Plugin Host");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // create controls from plugin->controls and add to window
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    GtkWidget *label = gtk_label_new(plugin->name.c_str());
    // large font for label


    // toggle to turn on/off the plugin
    GtkWidget *toggle = gtk_switch_new();

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), plugin->name.c_str());
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), plugin->description.c_str());
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), GTK_WIDGET(header));

    gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
    g_signal_connect(toggle, "state-set", G_CALLBACK(+[](GtkSwitch *widget, gboolean state, gpointer user_data) {
        Plugin *plugin = static_cast<Plugin *>(user_data);
        plugin->enabled = state;
    }), plugin);

    for (auto &control : plugin->controls) {
        LOGD("Creating control: %s, type: %d\n", control.name.c_str(), static_cast<int>(control.type));
        GtkWidget *label = gtk_label_new(control.name.c_str());
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        switch (control.type) {
            case lCONTROL:
            case lTOGGLE:
            case lTRIGGER: {
                GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, control.min, control.max, 0.01);
                gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_TOP);
                gtk_range_set_value(GTK_RANGE(scale), *(control.value));
                g_signal_connect(scale, "value-changed", G_CALLBACK(+[](GtkRange *range, gpointer user_data) {
                    Control *control = static_cast<Control *>(user_data);
                    *(control->value) = gtk_range_get_value(range);
                }), &control);
                gtk_box_pack_start(GTK_BOX(vbox), scale, FALSE, FALSE, 0);
                break;
            }
            case lAUDIO:
            case lMIDI:
                break;
            case lFILE: {
                GtkWidget *file_button = gtk_file_chooser_button_new(control.name.c_str(), GTK_FILE_CHOOSER_ACTION_OPEN);
                g_signal_connect(file_button, "file-set", G_CALLBACK(+[](GtkFileChooser *chooser, gpointer user_data) {
                    Control *control = static_cast<Control *>(user_data);
                    char *filename = gtk_file_chooser_get_filename(chooser);
                    if (filename != nullptr && control->plugin != nullptr) {
                        if (!control->plugin->sendFileNameToAtomPort(control->index, control->propertyUri, filename)) {
                            LOGE("Failed to send file path for %s\n", control->name.c_str());
                        }
                    }
                    g_free(filename);
                }), &control);
                gtk_box_pack_start(GTK_BOX(vbox), file_button, FALSE, FALSE, 0);
                break;
            }
            case lUNKNOWN:
                break;
        }
    }

    // Main execution thread idles here while the real-time thread runs
    LOGD("Entering GTK main loop\n");
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}