#include "night.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "config/app_config.h"
#include "hidemo.h"

#define tag "[night]: "

// bool night_mode_state = false;
//
//bool night_mode_enable() {
//    return false;
//    return night_mode_state;
//}

void export_pin(uint32_t pin, bool in) {
    char str_buf[64];
    {
        size_t len = sprintf(str_buf, "%d", pin);
        str_buf[len++] = 0;
        FILE *file = fopen("/sys/class/gpio/export", "w");
        fwrite(str_buf, len, 1, file);
        fclose(file);
    }
    {
        size_t len = sprintf(str_buf, "/sys/class/gpio/gpio%d/direction", pin);
        str_buf[len++] = 0;
        FILE *file = fopen(str_buf, "w");
        if (in)
            fwrite("in", 2, 1, file);
        else
            fwrite("out", 3, 1, file);
        fclose(file);
    }
}

void impulse_pin(uint32_t pin) {
    printf("impulse_pin on %d\n", pin);
    static char str_buf[64];
    {
        size_t len = sprintf(str_buf, "/sys/class/gpio/gpio%d/value", pin);
        str_buf[len++] = 0;
    }
    FILE *file = fopen(str_buf, "w");
    fwrite("1", 1, 1, file);
    fflush(file);
    usleep(250);
    fwrite("0", 1, 1, file);
    fflush(file);
    fclose(file);
}

bool get_pin_value(uint32_t pin) {
    static char str_buf[64];
    {
        size_t len = sprintf(str_buf, "/sys/class/gpio/gpio%d/value", pin);
        str_buf[len++] = 0;
    }
    char *v = "0"; {
        FILE *file = fopen(str_buf, "r");
        fread(v, 1, 1, file);
        fclose(file);
    }
    return v[0] == "1"[0];
}

void ir_cut_disable() {
    impulse_pin(app_config.ir_cut_disable_pin);
}
void ir_cut_enable() {
    impulse_pin(app_config.ir_cut_enable_pin);
}

void set_night_mode(bool night) {
    if (night) {
        printf("Change mode to NIGHT\n");
    } else {
        printf("Change mode to DAY\n");
    }

    if (night) {
        ir_cut_disable();
        set_color2gray(true);
    } else {
        ir_cut_enable();
        set_color2gray(false);
    }
}

extern bool keepRunning;

static night_mode = true;

void* night_thread_func(void *vargp)  {
    export_pin(app_config.ir_sensor_pin, true);
    export_pin(app_config.ir_cut_enable_pin, false);
    export_pin(app_config.ir_cut_disable_pin, false);

    set_night_mode(night_mode);

    while (keepRunning) {
        // bool current_night_mode = !night_mode;
        bool current_night_mode = get_pin_value(app_config.ir_sensor_pin);
        if (current_night_mode != night_mode) {
            set_night_mode(current_night_mode);
            night_mode = current_night_mode;
        }
        sleep(1);
    }
}

int32_t start_monitor_light_sensor() {
    pthread_t thread_id = 0;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr,&stacksize);
    size_t new_stacksize = 16*1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", new_stacksize); }
    pthread_create(&thread_id, &thread_attr, night_thread_func, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", stacksize); }
    pthread_attr_destroy(&thread_attr);
}
