#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

#define MESSAGE "Hello"

static struct mesh_conn mesh;
/*---------------------------------------------------------------------------*/
PROCESS(example_mesh_process, "Mesh example");
AUTOSTART_PROCESSES(&example_mesh_process);
/*---------------------------------------------------------------------------*/
static void
sent(struct mesh_conn *c)
{
  printf("packet sent\n");
}

static void
timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
}

static void
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
  printf("Data received from %d.%d: %.*s (%d)\n",
         from->u8[0], from->u8[1],
         packetbuf_datalen(), (char *)packetbuf_dataptr(), packetbuf_datalen());

  packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
  mesh_send(&mesh, from);
}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_mesh_process, ev, data)
{
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();

  mesh_open(&mesh, 132, &callbacks);

  SENSORS_ACTIVATE(button_sensor);

  while(1) {
    rimeaddr_t addr;

    /* Wait for button click before sending the first message. */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

    printf("Button clicked\n");

    /* Send a message to node number 1. */
    
    packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
    addr.u8[0] = 1;
    addr.u8[1] = 0;
    mesh_send(&mesh, &addr);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
