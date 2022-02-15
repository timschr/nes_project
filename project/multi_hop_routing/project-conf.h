#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

//Define the network stack
#define NETSTACK_CONF_NETWORK rime_driver // Define the network driver to use
#define NETSTACK_CONF_MAC     csma_driver // Define the MAC driver to use
#define NETSTACK_CONF_RDC     nullrdc_driver // Define the RDC driver to use. 
#define NETSTACK_CONF_FRAMER  framer_802154 // Define the framer driver to use
#define NETSTACK_CONF_RADIO   cc2420_driver // Define the radio driver to use. 

#undef TIMESYNCH_CONF_ENABLED  // TO ENABLE THE Implicit Network Time Synchronization
#define TIMESYNCH_CONF_ENABLED 1 // TO ENABLE THE Implicit Network Time Sync
