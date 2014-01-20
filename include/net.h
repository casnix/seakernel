#ifndef __SEA_NET_H
#define __SEA_NET_H

#include <types.h>
#include <ll.h>

struct net_dev {
	uint32_t flags;
	uint32_t state;
	size_t rx_count, tx_count, rx_err_count, tx_err_count, rx_pending;
	/* these fields are specified by the driver at time of net_dev creation */
	struct net_dev_calls *callbacks;
	void *data; /* driver specific data */
	
	struct llistnode *node;
};

struct net_packet {
	char data[0x1000];
	size_t length;
	size_t flags;
};

struct net_dev_calls {
	/* poll shall return received packets from the device in the array packets, up to
	 * the number specified by max. This call does not block, and will return no packets
	 * if none are available, and can return less than max packets if only some are
	 * available. 
	 */
	int (*poll)(struct net_dev *, struct net_packet *packets, int max);
	int (*set_flags)(struct net_dev *, uint32_t);
	int (*get_flags)(struct net_dev *, uint32_t *);
	int (*change_link)(struct net_dev *, uint32_t);
};

int net_callback_poll(struct net_dev *, struct net_packet *, int);
int net_callback_change_link(struct net_dev *, uint32_t);
int net_callback_set_flags(struct net_dev *, uint32_t);
int net_callback_get_flags(struct net_dev *, uint32_t *);

void net_notify_packet_ready(struct net_dev *nd);
int net_block_for_packets(struct net_dev *nd, struct net_packet *, int count);
struct net_dev *net_add_device(struct net_dev_calls *fn, void *);

#endif