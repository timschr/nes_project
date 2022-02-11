#include <stdio.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#define ON  1
#define OFF 0

PROCESS(led_pt, "Blink the LED");
PROCESS(btn_pt, "Handle button pressed");
PROCESS(energy_pt, "Energy estimation");

AUTOSTART_PROCESSES(&led_pt, &btn_pt, &energy_pt);

/* LED on/off durations */
static float duty_cycle = 0.1;
static double time_on;
static double time_off;

PROCESS_THREAD(btn_pt, ev, data) {
    PROCESS_BEGIN();
    
    SENSORS_ACTIVATE(button_sensor);
    
    // TODO: Implement here
    
    PROCESS_END();
}

PROCESS_THREAD(led_pt, ev, data) {
    static struct etimer timer;
    int state = OFF;
    PROCESS_BEGIN();
    time_on = 1/50 * duty_cycle * CLOCK_SECOND;
    time_off = 1/50 * (1-duty_cycle) * CLOCK_SECOND;
    printf("%f\n", time_on);
    printf("timer off %f\n", time_off);
    etimer_set(&timer, time_on);
    while(1) {
	if (state==OFF) {
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		leds_toggle(LEDS_RED);
		state = ON;
		etimer_set(&timer, time_off);
		etimer_reset(&timer);
	} else {
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
                leds_toggle(LEDS_RED);
                state = OFF;
                etimer_set(&timer, time_on);
                etimer_reset(&timer);
	}
    }
    
    PROCESS_END();
}

PROCESS_THREAD(energy_pt, ev, data) {
    PROCESS_BEGIN();
    
    static struct etimer et;
    
    /* Real-time clock */
    printf("RTIMER_SECOND: %u\n", RTIMER_SECOND);
    
    // TODO: Implement here
    
    PROCESS_END();
}
