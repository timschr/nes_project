#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#include <stdio.h>

#define MULTIHOP_CHANNEL 135
#define BROADCAST_CHANNEL 134
#define NOT_INIT 255

const linkaddr_t SINK_ADDRESS = {{1, 0}};


struct example_neighbor {
  struct example_neighbor *next;
  linkaddr_t addr;
  uint8_t num_hops;
  struct ctimer ctimer;
};


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

static void broadcast_distance_to_sink(uint8_t dist)
{
  packetbuf_copyfrom(&dist, sizeof(dist));
  broadcast_send(&broadcast);
}

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

static struct example_neighbor* get_closest_neighbour(const linkaddr_t* skip)
{
  // Find the closest neighbor to the sink
  struct example_neighbor *i, *closest;
  for(i = list_head(neighbor_table), closest = i; i != NULL; i = i->next) {
    if (closest->num_hops > i->num_hops && (!skip || (skip && !linkaddr_cmp(&i->addr, skip)))) {
      closest = i;
    }
  }

  return closest;
}

static uint8_t compute_shortest_path()
{
  struct example_neighbor *closest = get_closest_neighbour(NULL);

  if (closest != NULL)
  {
    return closest->num_hops + 1;
  }

  return NOT_INIT;
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
  if (linkaddr_cmp(&linkaddr_node_addr, &SINK_ADDRESS))
  {
    return;
  }

  struct example_neighbor *e;

  uint8_t rec_number_of_hops = *(uint8_t *)packetbuf_dataptr();

  /* We received an announcement from a neighbor so we need to update
     the neighbor list, or add a new entry to the table. */

  uint8_t current_shortest_path = compute_shortest_path();

  for(e = list_head(neighbor_table); e != NULL; e = e->next) {
    if(linkaddr_cmp(from, &e->addr)) {
      /* Our neighbor was found, so we update the timeout. */
      ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e); 
      if (rec_number_of_hops != NOT_INIT) {
        e->num_hops = rec_number_of_hops;
        if (current_shortest_path > rec_number_of_hops || current_shortest_path == NOT_INIT) {
          broadcast_distance_to_sink(rec_number_of_hops + 1);
        }
      }
      // else {
      //     printf("Neighbor %d doesn't know where sink is\n", from->u8[0]);
      // }
      return;
    }
  }

  /* The neighbor was not found in the list, so we add a new entry by
     allocating memory from the neighbor_mem pool, fill in the
     necessary fields, and add it to the list. */
  e = memb_alloc(&neighbor_mem);
  if(e != NULL) {
    e->num_hops = rec_number_of_hops;
    linkaddr_copy(&e->addr, from);
    list_add(neighbor_table, e);
    ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);

    if (rec_number_of_hops != NOT_INIT) {
        if (current_shortest_path > rec_number_of_hops || current_shortest_path == NOT_INIT) {
          broadcast_distance_to_sink(rec_number_of_hops + 1);
        }
    }
    // else {
    //     printf("Neighbor %d not known yet, but doesn't know where sink is\n", from->u8[0]);
    // }

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
  printf("multihop message received from %d.%d: '%s' (# hops: %d)\n", sender->u8[0], sender->u8[1], (char*)packetbuf_dataptr(), hops);
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
  struct example_neighbor* closest = get_closest_neighbour(prevhop);

  if (closest != NULL && closest->num_hops != NOT_INIT && !linkaddr_cmp(&closest->addr, prevhop)) {
    printf("%d.%d: Forwarding packet to %d.%d, hops %d\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
             closest->addr.u8[0], closest->addr.u8[1],
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
    
  PROCESS_BEGIN();

  /* Open a multihop connection on Rime channel MULTIHOP_CHANNEL. */
  multihop_open(&multihop, MULTIHOP_CHANNEL, &multihop_call);
  
  /* Activate the button sensor. We use the button to drive traffic -
     when the button is pressed, a packet is sent. */
  SENSORS_ACTIVATE(button_sensor);

  /* Loop forever, send a packet when the button is pressed. */
  while(1) {
    /* Wait until we get a sensor event with the button sensor as data. */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
                             data == &button_sensor);

    /* Copy the "EMERGENCY" to the packet buffer. */
    packetbuf_copyfrom("EMERGENCY", 10);

    /* Set the Rime address of the final receiver of the packet to
       1.0. This is a value that happens to work nicely in a Cooja
       simulation (because the default simulation setup creates one
       node with address 1.0). */

    /* Send the packet. */
    multihop_send(&multihop, &SINK_ADDRESS);
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
   */
  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_cb);

  // First node is the sink and thus its distance to the sink is 0
  if (linkaddr_cmp(&linkaddr_node_addr, &SINK_ADDRESS)){
    unsigned long ticks = CLOCK_SECOND * 5;
    etimer_set(&et, ticks);

    // Wait for all the nodes to be initialized
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    broadcast_distance_to_sink(0);
  }

  // Broadcast distance to sink to neighbours 3 times more frequently than
  // the neighbour timeout. This indicates that the node is still there and 
  // communicating.
  while(1) {
    etimer_set(&et, NEIGHBOR_TIMEOUT / 3);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    broadcast_distance_to_sink(compute_shortest_path());
  }

  PROCESS_END();
}
