/* Some of this code is stolen from: http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/Samples/AsyncDeviceList/

PulseAudio docs: http://www.freedesktop.org/software/pulseaudio/doxygen/
*/

#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <libappindicator/app-indicator.h>
#include <libnotify/notify.h>

enum {
    JOAPA_SINK, JOAPA_SOURCE
};

// Information about a port
typedef struct pa_portlist {
    char name[512];
    char description[256];
    int available;
    uint8_t initialized;
} pa_portlist_t;

// Information about a device
typedef struct pa_devicelist {
        char name[512];
        char description[256];
        int type;
        uint8_t initialized;
        uint32_t index;
        uint32_t n_ports;
        pa_portlist_t ports[10];
        pa_portlist_t active_port;
} pa_devicelist_t;

// Combination of the two above
typedef struct pa_device_port {
    pa_devicelist_t device;
    pa_portlist_t port;
} pa_device_port_t;

typedef struct pa_clientlist {
    uint32_t index;
    int initialized;
} pa_clientlist_t;

static void activate_action (GtkWidget *button, pa_device_port_t *data);

void pa_sink_info_cb(pa_context *c, const pa_sink_input_info *l, int eol, void *userdata);
void pa_source_info_cb(pa_context *c, const pa_source_output_info *l, int eol, void *userdata);

void set_active_port_cb(pa_context *c, int success, void *userdata);
int set_active_port(pa_devicelist_t device, pa_portlist_t port);

void pa_state_cb(pa_context *c, void *userdata);
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output);

void init_app_indicator(int argc, char **argv);
void fill_app_indicator(pa_devicelist_t *input, pa_devicelist_t *output);

AppIndicator *indicator;
GtkWidget *menu;

int main(int argc, char *argv[]) {
    pa_devicelist_t pa_input_devicelist[16];
    pa_devicelist_t pa_output_devicelist[16];

    if (pa_get_devicelist(pa_input_devicelist, pa_output_devicelist) < 0) {
        fprintf(stderr, "failed to get device list\n");
        return 1;
    }
    
    init_app_indicator(argc, argv);
    fill_app_indicator(pa_input_devicelist, pa_output_devicelist);
    
    gtk_main ();
    
    return 0;
}

static void activate_action (GtkWidget *button, pa_device_port_t *data) {
    char str[512];
    strcpy(str, data->device.description);
    strcat(str, "\n");
    strcat(str, data->port.description);
    
    notify_init ("PA device switcher");
	NotifyNotification * notification = notify_notification_new ("PA device switcher", 
	    str, 
	    data->port.description);
	notify_notification_show (notification, NULL);
	
    set_active_port(data->device, data->port);
}

void create_list_for_type(pa_devicelist_t *list) {
    int i, j;
    for(i = 0; i < 16; i++) {
        if(!list[i].initialized)
            break;
        
        for(j = 0; j < list[i].n_ports; j++) {
            GtkWidget *widget = gtk_menu_item_new_with_label(list[i].ports[j].description);
            gtk_menu_append(GTK_MENU_SHELL (menu),widget);
            gtk_widget_show(widget);
            
            // HER VAR VI. SEGFAULT SOMEWHERE
            pa_device_port_t *dp = malloc(sizeof(pa_device_port_t));
            dp->device = list[i];
            dp->port = list[i].ports[j];
            
            g_signal_connect(G_OBJECT(widget), "activate", G_CALLBACK(activate_action), dp);
        }
    }
}

void add_separator_to_menu() {
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_append(GTK_MENU_SHELL (menu),separator);
    gtk_widget_show(separator);
}

void fill_app_indicator(pa_devicelist_t *input, pa_devicelist_t *output) {
    gtk_widget_destroy(menu);
    GtkWidget *menu_items[3];
    
    menu = gtk_menu_new();
    
    create_list_for_type(input);
    add_separator_to_menu();
    create_list_for_type(output);
    add_separator_to_menu();
    
    GtkWidget *btn_quit = gtk_menu_item_new_with_label("Quit");
    gtk_menu_append(GTK_MENU_SHELL (menu),btn_quit);
    gtk_widget_show(btn_quit);
    
    g_signal_connect (btn_quit, "activate",
            G_CALLBACK (gtk_main_quit), NULL);
                    
    app_indicator_set_menu (indicator, GTK_MENU (menu));
}

void init_app_indicator(int argc, char **argv) {
    GtkWidget *window;
    
    GError *error = NULL;

    gtk_init (&argc, &argv);

    /* main window  */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect (G_OBJECT (window),
            "destroy",
            G_CALLBACK (gtk_main_quit),
            NULL);

    /* Indicator */
    indicator = app_indicator_new ("pa-device-switcher-indicator",
            "indicator-messages",
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    menu = gtk_menu_new();
    
    app_indicator_set_status (indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon (indicator, "indicator-messages-new");

    app_indicator_set_menu (indicator, GTK_MENU (menu));
}

// Callback for set_active_port
void set_active_port_cb(pa_context *c, int success, void *userdata) {
    //pa_device_port_t set = *userdata;
    //printf("%s %s", set.device.description, set.port.description);
}

int set_active_port(pa_devicelist_t device, pa_portlist_t port) {
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_operation *pa_op;
    pa_context *pa_ctx;
    
    int pa_ready = 0;
    int state = 0;
    
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);
    
    pa_device_port_t dev_port_set;
    dev_port_set.device = device;
    dev_port_set.port = port;
    
    pa_clientlist_t clientlist[30];
    
    int i = 0;
    
    for (;;) {
        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        
        switch(state) {
            case 0:
                // Set source or sink
                switch(device.type) {
                    case JOAPA_SOURCE:
                        pa_op = pa_context_set_source_port_by_index(
                                pa_ctx, 
                                device.index, 
                                port.name, 
                                set_active_port_cb, 
                                &dev_port_set);
                        break;
                    case JOAPA_SINK:
                        pa_op = pa_context_set_sink_port_by_index(
                                pa_ctx, 
                                device.index, 
                                port.name, 
                                set_active_port_cb, 
                                &dev_port_set);
                        break;
                }
                state++;
                break;
            case 1:
                // get clients using a source or sink
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    switch(device.type) {
                        case JOAPA_SOURCE:
                            pa_op = pa_context_get_source_output_info_list(pa_ctx, pa_source_info_cb, &clientlist);
                            break;
                        case JOAPA_SINK:
                            pa_op = pa_context_get_sink_input_info_list(pa_ctx, pa_sink_info_cb, &clientlist);
                            break;
                    }
                    state++;
                }
                break;
            case 2:
                // move the clients to the new source or sink
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    
                    if(!clientlist[i].initialized) {
                        pa_context_disconnect(pa_ctx);
                        pa_context_unref(pa_ctx);
                        pa_mainloop_free(pa_ml);
                        return 0;
                    }
                    //printf("CLIENT: %d\n", clientlist[i].index);
                    
                    switch(device.type) {
                        case JOAPA_SOURCE:
                            pa_op = pa_context_move_source_output_by_index(
                                    pa_ctx,
                                    clientlist[i].index, 
                                    device.index, 
                                    set_active_port_cb, NULL);
                            break;
                        case JOAPA_SINK:
                            pa_op = pa_context_move_sink_input_by_index(
                                    pa_ctx, 
                                    clientlist[i].index, 
                                    device.index, 
                                    set_active_port_cb, NULL);
                            break;
                    }
                    
                    i++;
                }
                break;
            default:
                fprintf(stderr, "in state %d\n", state);
                return -1;
        }
        pa_mainloop_iterate(pa_ml, 1, NULL);       
    }

}

int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_operation *pa_op;
    pa_context *pa_ctx;

    // We'll need these state variables to keep track of our requests
    int state = 0;
    int pa_ready = 0;

    // Initialize our device lists
    memset(input, 0, sizeof(pa_devicelist_t) * 16);
    memset(output, 0, sizeof(pa_devicelist_t) * 16);

    // Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");

    // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    for (;;) {
        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        // At this point, we're connected to the server and ready to make
        // requests
        switch (state) {
            // State 0: we haven't done anything yet
            case 0:
                // This sends an operation to the server.  pa_sinklist_info is
                // our callback function and a pointer to our devicelist will
                // be passed to the callback The operation ID is stored in the
                // pa_op variable
                pa_op = pa_context_get_sink_info_list(pa_ctx,
                        pa_sinklist_cb,
                        output
                        );

                // Update state for next iteration through the loop
                state++;
                break;
            
            case 1:
                // Now we wait for our operation to complete.  When it's
                // complete our pa_output_devicelist is filled out, and we move
                // along to the next state
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);

                    // Now we perform another operation to get the source
                    // (input device) list just like before.  This time we pass
                    // a pointer to our input structure
                    pa_op = pa_context_get_source_info_list(pa_ctx,
                            pa_sourcelist_cb,
                            input
                            );
                    // Update the state so we know what to do next
                    state++;
                }
                break;
            case 2:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    // Now we're done, clean up and disconnect and return
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
                    return 0;
                }
                break;
            default:
                // We should never see this state
                fprintf(stderr, "in state %d\n", state);
                return -1;
        }
        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata) {
        pa_context_state_t state;
        int *pa_ready = userdata;

        state = pa_context_get_state(c);
        switch  (state) {
                // There are just here for reference
                case PA_CONTEXT_UNCONNECTED:
                case PA_CONTEXT_CONNECTING:
                case PA_CONTEXT_AUTHORIZING:
                case PA_CONTEXT_SETTING_NAME:
                default:
                        break;
                case PA_CONTEXT_FAILED:
                case PA_CONTEXT_TERMINATED:
                        *pa_ready = 2;
                        break;
                case PA_CONTEXT_READY:
                        *pa_ready = 1;
                        break;
        }
}

// pa_mainloop will call this function when it's ready to tell us about a sink.
// Since we're not threading, there's no need for mutexes on the devicelist
// structure
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = userdata;
    int ctr = 0, i = 0;

    if (eol > 0) {
        return;
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_devicelist[ctr].initialized) {
            strncpy(pa_devicelist[ctr].name, l->name, 511);
            strncpy(pa_devicelist[ctr].description, l->description, 255);
            pa_devicelist[ctr].index = l->index;
            pa_devicelist[ctr].initialized = 1;
            pa_devicelist[ctr].type = JOAPA_SINK;
            
            if(l->n_ports > 0) {
                for(i = 0; (i < l->n_ports) && pa_devicelist[ctr].n_ports < 10; i++) {
                    strncpy(pa_devicelist[ctr].ports[i].name, l->ports[i]->name, 511);
                    strncpy(pa_devicelist[ctr].ports[i].description, l->ports[i]->description, 255);
                    pa_devicelist[ctr].n_ports++;
                }
                strncpy(pa_devicelist[ctr].active_port.name,  l->active_port->name, 511);
                strncpy(pa_devicelist[ctr].active_port.description, l->active_port->description, 255);
            }
            break;
        }
    }
}

// See above.  This callback is pretty much identical to the previous
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = userdata;
    int ctr = 0, i = 0;

    if (eol > 0) {
        return;
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_devicelist[ctr].initialized) {
            strncpy(pa_devicelist[ctr].name, l->name, 511);
            strncpy(pa_devicelist[ctr].description, l->description, 255);
            pa_devicelist[ctr].index = l->index;
            pa_devicelist[ctr].initialized = 1;
            pa_devicelist[ctr].type = JOAPA_SOURCE;
            
            if(l->n_ports > 0) {
                for(i = 0; (i < l->n_ports) && pa_devicelist[ctr].n_ports < 10; i++) {
                    strncpy(pa_devicelist[ctr].ports[i].name, l->ports[i]->name, 511);
                    strncpy(pa_devicelist[ctr].ports[i].description, l->ports[i]->description, 255);
                    pa_devicelist[ctr].n_ports++;
                }
                strncpy(pa_devicelist[ctr].active_port.name,  l->active_port->name, 511);
                strncpy(pa_devicelist[ctr].active_port.description, l->active_port->description, 255);
            }
            break;
        }
    }
}

void pa_sink_info_cb(pa_context *c, const pa_sink_input_info *l, int eol, void *userdata) {
    pa_clientlist_t *clientlist = userdata;
    
    int ctr = 0;
    
    if (eol > 0) {
        return;
    }
    
    for(ctr = 0; ctr < 30; ctr++) {
        if(!clientlist[ctr].initialized) {
            clientlist[ctr].index = l->index;
            clientlist[ctr].initialized = 1;
            break;
        }
    }
}

void pa_source_info_cb(pa_context *c, const pa_source_output_info *l, int eol, void *userdata) {
    pa_clientlist_t *clientlist = userdata;
    
    int ctr = 0;
    
    if (eol > 0) {
        return;
    }
    
    for(ctr = 0; ctr < 30; ctr++) {
        if(!clientlist[ctr].initialized) {
            clientlist[ctr].index = l->index;
            clientlist[ctr].initialized = 1;
            break;
        }
    }
}
