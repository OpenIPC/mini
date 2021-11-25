#include "night.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "config/app_config.h"
#include "hidemo.h"

#include "gpio.h"

#define tag "[night]: "

static bool night_mode = false;

bool night_mode_is_enable() { return night_mode; }

void ircut_on() {
    set_pin_linux(app_config.ir_cut_pin1, false);
    set_pin_linux(app_config.ir_cut_pin2, true);
    usleep(app_config.pin_switch_delay_us);
    set_pin_linux(app_config.ir_cut_pin1, false);
    set_pin_linux(app_config.ir_cut_pin2, false);
    set_color2gray(true);
}

void ircut_off() {
    set_pin_linux(app_config.ir_cut_pin1, true);
    set_pin_linux(app_config.ir_cut_pin2, false);
    usleep(app_config.pin_switch_delay_us);
    set_pin_linux(app_config.ir_cut_pin1, false);
    set_pin_linux(app_config.ir_cut_pin2, false);
    set_color2gray(false);
}

void set_night_mode(bool night) {
    if (night) {
        printf("Change mode to NIGHT\n");
        ircut_off();
        set_color2gray(true);
    } else {
        printf("Change mode to DAY\n");
        ircut_on();
        set_color2gray(false);
    }
}

extern bool keepRunning;

void *night_thread_func(void *vargp) {
    usleep(1000);
    set_night_mode(night_mode);

    while (keepRunning) {
        bool state = false;
        if (!get_pin_linux(app_config.ir_sensor_pin, &state)) {
            printf(tag "get_pin_linux(app_config.ir_sensor_pin) error\n");
            sleep(app_config.check_interval_s);
            continue;
        }
        if (night_mode != state) {
            night_mode = state;
            set_night_mode(night_mode);
        }
        sleep(app_config.check_interval_s);
    }
}

int32_t start_monitor_light_sensor() {
    pthread_t thread_id = 0;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
        printf(tag "Error:  Can't set stack size %ld\n", new_stacksize);
    }
    pthread_create(&thread_id, &thread_attr, night_thread_func, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
        printf(tag "Error:  Can't set stack size %ld\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}
