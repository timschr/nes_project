#include <stdio.h>
#include "net/rime/rime.h"
#include "random.h"
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

PROCESS(init_rx_pt, "Recieve hello");
PROCESS(hello_pt, "Send hello");

AUTOSTART_PROCESSES(&init_rx_pt);

static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
    /* 
     * Function: void* packetbuf_dataptr()
     * Get a pointer to the data in the packetbuf
     * Using appropriate casting operator for data
     */
    printf("broadcast message received from %d.%d: '%s'\n", 
           from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

static const struct broadcast_callbacks broadcast_cb = {broadcast_recv};
static struct broadcast_conn broadcast;

PROCESS_THREAD(hello, ev, data) {
	static struct etimer et;
//	PROCESS_EXITHANDLER(broadcast_close(&broadcast);

	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor);
	PROCESS_WAIT_EVENT_UNTIL(data == &button_sensor);
	/*
     	* Set up a broadcast connection
     	* Arguments: channel (129) and callbacks function
     	*/
    	broadcast_open(&broadcast, 129, &broadcast_cb);

	/* Send broadcast packet in loop */
	while(1) {
       		/* Delay 2-4 seconds*/
        	unsigned long ticks = CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 2);
        	etimer_set(&et, ticks);
       		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		/* Copy data to the packet buffer */
       		packetbuf_copyfrom("Hello", 6);
        	/* Send broadcast packet */
        	broadcast_send(&broadcast);
        	printf("broadcast packet sent\n");
    	}

	PROCESS_END();

}
