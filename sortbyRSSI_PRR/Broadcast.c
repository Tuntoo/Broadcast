#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/leds.h"
#include <stdio.h>
#include <string.h>

#define MAX_NEIGHBORS 5
#define MAX_ROUTE_LEN 64

PROCESS(example_broadcast_process, "Broadcast Example");
PROCESS(decrease_counter_process, "Decrease RX Counter");
AUTOSTART_PROCESSES(&example_broadcast_process, &decrease_counter_process);

typedef struct {
  uint8_t id_0;
  int rssi;
  int rx_counter;
} Neighbor;

static Neighbor neighbors[MAX_NEIGHBORS];
static struct broadcast_conn broadcast;

void sort_neighbors_by_rssi() {
  int i, j;
  for (i = 0; i < MAX_NEIGHBORS - 1; i++) {
    for (j = i + 1; j < MAX_NEIGHBORS; j++) {
      if (neighbors[i].rssi < neighbors[j].rssi) {
        Neighbor temp = neighbors[i];
        neighbors[i] = neighbors[j];
        neighbors[j] = temp;
      }
    }
  }
}

void add_or_update_neighbor(uint8_t id, int rssi) {
  int i;
  for (i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].id_0 == id) {
      neighbors[i].rssi = rssi;
      neighbors[i].rx_counter = 5;
      sort_neighbors_by_rssi();
      return;
    }
  }
  for (i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].id_0 == 0) {
      neighbors[i].id_0 = id;
      neighbors[i].rssi = rssi;
      neighbors[i].rx_counter = 5;
      sort_neighbors_by_rssi();
      return;
    }
  }
  if (rssi > neighbors[MAX_NEIGHBORS - 1].rssi) {
    neighbors[MAX_NEIGHBORS - 1].id_0 = id;
    neighbors[MAX_NEIGHBORS - 1].rssi = rssi;
    neighbors[MAX_NEIGHBORS - 1].rx_counter = 5;
    sort_neighbors_by_rssi();
  }
}

void remove_inactive_neighbors() {
  int i;
  for (i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].rx_counter <= 0) {
      memset(&neighbors[i], 0, sizeof(Neighbor));
    }
  }
}

int is_in_route(const char *msg, uint8_t id) {
  char id_str[4];
  sprintf(id_str, "-%d", id);
  return strstr(msg, id_str) != NULL;
}

void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
  int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  add_or_update_neighbor(from->u8[0], rssi);

  char *msg = (char *)packetbuf_dataptr();
  if (linkaddr_node_addr.u8[0] == 1) {
	  snprintf(msg, sizeof(msg), "%d", linkaddr_node_addr.u8[0]);
	  packetbuf_copyfrom(msg, strlen(msg) + 1);
	  broadcast_send(&broadcast);
	  printf("Node %d sent initial packet\n", linkaddr_node_addr.u8[0]);
	}
  if (is_in_route(msg, linkaddr_node_addr.u8[0])) {
    return;
  }

  char new_msg[MAX_ROUTE_LEN];
  snprintf(new_msg, sizeof(new_msg), "%s-%d", msg, linkaddr_node_addr.u8[0]);

	if (linkaddr_node_addr.u8[0] == 10) {
	  if (strncmp(new_msg, "1-", 2) == 0 || strcmp(new_msg, "1") == 0) {
		printf("Valid Final route: %s\n", new_msg);
	  }
	  return;
	}

  int i;
  for (i = 0; i < MAX_NEIGHBORS; i++) {
    if (neighbors[i].id_0 != 0 && !is_in_route(new_msg, neighbors[i].id_0)) {
      packetbuf_copyfrom(new_msg, strlen(new_msg) + 1);
      broadcast_send(&broadcast);
      break;
    }
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

PROCESS_THREAD(example_broadcast_process, ev, data) {
  static struct etimer et;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);

  while (1) {
	etimer_set(&et, CLOCK_SECOND * 10 + random_rand() % (CLOCK_SECOND * 4));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    char msg[MAX_ROUTE_LEN];
    snprintf(msg, sizeof(msg), "%d", linkaddr_node_addr.u8[0]);
    packetbuf_copyfrom(msg, strlen(msg) + 1);
    broadcast_send(&broadcast);
    printf("Node %d sent initial packet\n", linkaddr_node_addr.u8[0]);
  }

  PROCESS_END();
}

PROCESS_THREAD(decrease_counter_process, ev, data) {
  static struct etimer et;
  int i;
  PROCESS_BEGIN();

  while (1) {
    etimer_set(&et, CLOCK_SECOND * 2);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    for (i = 0; i < MAX_NEIGHBORS; i++) {
      if (neighbors[i].rx_counter > 0) {
        neighbors[i].rx_counter--;
      }
    }
    remove_inactive_neighbors();
  }

  PROCESS_END();
}