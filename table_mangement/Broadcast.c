#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/leds.h"
#include <stdio.h>

#define MAX_NEIGHBORS 5 


PROCESS(example_broadcast_process, "Broadcast Example");
AUTOSTART_PROCESSES(&example_broadcast_process);

typedef struct {
    uint8_t id_0;  
    int rssi;
    int prr;
	int rx_counter;
    int rx_packets;
    int tx_packets;
} Neighbor;

static int tx_counter;
static Neighbor neighbors[MAX_NEIGHBORS];
static struct broadcast_conn broadcast;


int find_neighbor(uint8_t id_0) {
	int i;
    for (i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].id_0 == id_0) {
            return i; 
        }
    }
    return -1; 
}


void update_rx_packets(uint8_t id_0, int rssi) {
    int index = find_neighbor(id_0);
    int i;
    if (index != -1) { // if there was already a mote
        neighbors[index].rx_packets++;
        neighbors[index].rssi = rssi;
        if (neighbors[index].tx_packets > 0) {
            neighbors[index].prr = neighbors[index].rx_packets * 100 / neighbors[index].tx_packets;
        }
        if (neighbors[index].rx_counter < 30) {
            neighbors[index].rx_counter += 10;  // increase the rx_counter which is received packets
        }
    } else { // add new
        for (i = 0; i < MAX_NEIGHBORS; i++) {
            if (neighbors[i].id_0 == 0) { 
                neighbors[i].id_0 = id_0;
                neighbors[i].rssi = rssi;
                neighbors[i].rx_counter = 20;
                neighbors[i].rx_packets = 1;
                neighbors[i].tx_packets = tx_counter;
                return;
            }
        }
    }
}


void update_tx_packets(){
	int i;
    for ( i = 0; i < MAX_NEIGHBORS; i++) {
		neighbors[i].tx_packets = tx_counter;
    }
}

void sort_neighbors_by_rssi() {
	int i,j;
    for ( i = 0; i < MAX_NEIGHBORS - 1; i++) {
        for ( j = i + 1; j < MAX_NEIGHBORS; j++) {
            if (neighbors[i].rssi < neighbors[j].rssi) {
                Neighbor temp = neighbors[i];
                neighbors[i] = neighbors[j];
                neighbors[j] = temp;
            }
        }
    }
}


void remove_inactive_neighbors() {
    int i;
    for (i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].id_0 != 0) { // find mote
            if (neighbors[i].rx_counter > 0) {
                neighbors[i].rx_counter--;  // decrease rx_counter
            }
            if (neighbors[i].rx_counter == 0) {
                memset(&neighbors[i], 0, sizeof(Neighbor)); // remove mote if cant find in a times
            }
        }
    }
}
void convert_inttofloat(int num, int *quotient, int *remainder) {
    *quotient = num / 100;
    *remainder = num % 100;
}


void print_neighbors_table() {
	int i,quotient, remainder;;
    sort_neighbors_by_rssi();
    printf("\n-------------------------------------\n");
    printf("| Node_ID | RSSI | PRR | RX_Counter | RX_Packets | TX_Packets |\n");
    printf("-------------------------------------\n");
    for ( i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].id_0 != 0 ) {
			    convert_inttofloat(neighbors[i].prr, &quotient, &remainder);
            printf("|  %d      | %d  | %d.%d  |  %d        |  %d      |  %d      | \n",
                   neighbors[i].id_0,neighbors[i].rssi,quotient,remainder,neighbors[i].rx_counter,neighbors[i].rx_packets,neighbors[i].tx_packets);
        }
    }
    printf("-------------------------------------\n\n");
}

/* Callback recive packet */
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
    int16_t  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	printf("%d       %d", rssi, from->u8[0]);
    update_tx_packets();
    update_rx_packets(from->u8[0], rssi);  
    remove_inactive_neighbors();          
    print_neighbors_table();
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/* Main process */
PROCESS_THREAD(example_broadcast_process, ev, data) {
    static struct etimer et;
    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();

    broadcast_open(&broadcast, 129, &broadcast_call);

    while (1) {
        etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		printf(" %d is sent",linkaddr_node_addr.u8[0]);

        packetbuf_copyfrom(" ", 6);
        broadcast_send(&broadcast);
		tx_counter++;
    }

    PROCESS_END();
}
