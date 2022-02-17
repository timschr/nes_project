#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#include <stdio.h>

#define CHANNEL 135
#define NOT_INIT 255


struct example_neighbor {
  struct example_neighbor *next;
  linkaddr_t addr;
  uint8_t num_hops;
  struct ctimer ctimer;
};

static uint8_t sink_hops;


static struct broadcast_conn broadcast;

#define NEIGHBOR_TIMEOUT 60 * CLOCK_SECOND
#define MAX_NEIGHBORS 16
LIST(neighbor_table);
MEMB(neighbor_mem, struct example_neighbor, MAX_NEIGHBORS);
/*---------------------------------------------------------------------------*/
PROCESS(example_multihop_process, "multihop example");
PROCESS(broadcast_process, "broadcast");

AUTOSTART_PROCESSES(&example_multihop_process, &broadcast_process);
/*---------------------------------------------------------------------------*/
/*
 * This function is called by the ctimer present in each neighbor
 * table entry. The function removes the neighbor from the table
 * because it has become too old.
 */
static void
remove_neighbor(void *n)
{
  struct example_neighbor *e = n;

  list_remove(neighbor_table, e);
  memb_free(&neighbor_mem, e);
}

/*---------------------------------------------------------------------------*/
/*
 * This function is called when an incoming announcement arrives. The
 * function checks the neighbor table to see if the neighbor is
 * already present in the list. If the neighbor is not present in the
 * list, a new neighbor table entry is allocated and is added to the
 * neighbor table.
 */
static void
received_broadcast(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct example_neighbor *e;

  uint8_t rec_number_of_hops = *(uint8_t *)packetbuf_dataptr();

  /* We received an announcement from a neighbor so we need to update
     the neighbor list, or add a new entry to the table. */
  for(e = list_head(neighbor_table); e != NULL; e = e->next) {
    if(linkaddr_cmp(from, &e->addr)) {
      /* Our neighbor was found, so we update the timeout. */
      ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e); 
      if (rec_number_of_hops != NOT_INIT) {
        if (sink_hops > rec_number_of_hops || sink_hops == NOT_INIT) {
          sink_hops = rec_number_of_hops + 1;
          e->num_hops = rec_number_of_hops;
          
          packetbuf_copyfrom(&sink_hops, sizeof(sink_hops));
          broadcast_send(&broadcast);

          printf("Updated #Hops to sink %d\n", sink_hops);
          printf("sink_hops: %d, e->num_hops: %d, rec_number_of_hops: %d\n", sink_hops, e->num_hops, rec_number_of_hops);
        }
      } else {
          printf("Neighbor %d doesn't know where sink is\n", from->u8[0]);
      }
      return;
    }
  }

  /* The neighbor was not found in the list, so we add a new entry by
     allocating memory from the neighbor_mem pool, fill in the
     necessary fields, and add it to the list. */
  e = memb_alloc(&neighbor_mem);
  if(e != NULL) {
    if (rec_number_of_hops != NOT_INIT) {
        if (sink_hops > rec_number_of_hops || sink_hops == NOT_INIT) {
          sink_hops = rec_number_of_hops + 1;

          packetbuf_copyfrom(&sink_hops, sizeof(sink_hops));
          broadcast_send(&broadcast);

          printf("Updated #Hops to sinks %d\n", sink_hops);
        }
    } else {
        printf("Neighbor %d not known yet, but doesn't know where sink is\n", from->u8[0]);
    }

    e->num_hops = rec_number_of_hops;
    linkaddr_copy(&e->addr, from);
    list_add(neighbor_table, e);
    ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);
  }
}

/*---------------------------------------------------------------------------*/
/*
 * This function is called at the final recepient of the message.
 */
static void
recv(struct multihop_conn *c, const linkaddr_t *sender,
     const linkaddr_t *prevhop,
     uint8_t hops)
{
  // TODO: Print the sender
  printf("multihop message received '%s'\n", (char*)packetbuf_dataptr());
}
/*
 * This function is called to forward a packet. The function picks a
 * random neighbor from the neighbor list and returns its address. The
 * multihop layer sends the packet to this address. If no neighbor is
 * found, the function returns NULL to signal to the multihop layer
 * that the packet should be dropped.
 */
static linkaddr_t *
forward(struct multihop_conn *c,
        const linkaddr_t *originator, const linkaddr_t *dest,
        const linkaddr_t *prevhop, uint8_t hops)
{
  // Find the closest neighbor to the sink
  struct example_neighbor *i, *closest;
  printf("Looking for a shortest path: %d.\n", sink_hops);
  for(i = list_head(neighbor_table), closest = i; i != NULL; i = i->next) {
    printf("Iterating through neighbour %d, distance to sink: %d, current shortest distance: %d\n", i->addr.u8[0], i->num_hops, closest->num_hops);
    if (closest->num_hops > i->num_hops) {
      printf("Path is shorter. Overriding closest.\n");
      closest = i;
    }
  }

  if (closest != NULL) {
    printf("%d.%d: Forwarding packet to %d.%d (%d in list), hops %d\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
             closest->addr.u8[0], closest->addr.u8[1], closest->num_hops,
             packetbuf_attr(PACKETBUF_ATTR_HOPS));
    return &closest->addr;
  }

  printf("%d.%d: did not find a neighbor to forward to\n",
         linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  return NULL;
}

static const struct multihop_callbacks multihop_call = {recv, forward};
static struct multihop_conn multihop;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_multihop_process, ev, data)
{
  PROCESS_EXITHANDLER(multihop_close(&multihop);)

  static struct etimer et;
    
  PROCESS_BEGIN();

  /* Initialize the memory for the neighbor table entries. */
  memb_init(&neighbor_mem);

  /* Initialize the list used for the neighbor table. */
  list_init(neighbor_table);

  /* Open a multihop connection on Rime channel CHANNEL. */
  multihop_open(&multihop, CHANNEL, &multihop_call);

  if ((linkaddr_node_addr.u8[0] == 1) && (linkaddr_node_addr.u8[1] == 0)){
    sink_hops = 0;
  }
  else {
    sink_hops = NOT_INIT;
  }
  
  /* Activate the button sensor. We use the button to drive traffic -
     when the button is pressed, a packet is sent. */
  SENSORS_ACTIVATE(button_sensor);

  /* Loop forever, send a packet when the button is pressed. */
  while(1) {
    linkaddr_t to;

    /* Wait until we get a sensor event with the button sensor as data. */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
                             data == &button_sensor);

    /* Copy the "Hello" to the packet buffer. */
    packetbuf_copyfrom("EMERGENCY", 10);

    /* Set the Rime address of the final receiver of the packet to
       1.0. This is a value that happens to work nicely in a Cooja
       simulation (because the default simulation setup creates one
       node with address 1.0). */
    to.u8[0] = 1;
    to.u8[1] = 0;

    /* Send the packet. */
    multihop_send(&multihop, &to);

  }

  PROCESS_END();
}

static const struct broadcast_callbacks broadcast_cb = {received_broadcast};

PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  /* Initialize the memory for the neighbor table entries. */
  memb_init(&neighbor_mem);

  /* Initialize the list used for the neighbor table. */
  list_init(neighbor_table);
  
  /* 
   * Set up a broadcast connection
   * Arguments: channel (129) and callbacks function
   */
  broadcast_open(&broadcast, 123, &broadcast_cb);

  if ((linkaddr_node_addr.u8[0] == 1) && (linkaddr_node_addr.u8[1] == 0)){
    sink_hops = 0;
    /* Copy data to the packet buffer */
    packetbuf_copyfrom(&sink_hops, sizeof(sink_hops));

    unsigned long ticks = CLOCK_SECOND * 5;
    
    etimer_set(&et, ticks);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    /* Send broadcast packet */
    broadcast_send(&broadcast);
  }
  else {
    sink_hops = NOT_INIT;
  }


  while(1) {
    etimer_set(&et, NEIGHBOR_TIMEOUT / 5);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom(&sink_hops, sizeof(sink_hops));
    broadcast_send(&broadcast);
  }

  PROCESS_END();
}
