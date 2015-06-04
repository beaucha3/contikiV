/*

File: One of the main source files for the air quality monitoring project.
When compiled, this file's program module is meant to be loaded onto the SKY motes that form the grid network.


Author: Nathan Beauchamp (beaucha3@illinois.edu)

with assistance from Isaac Kousari (kousari2@illinois.edu)

Institution: University of Illinois at Urbana-Champaign, Coordinated Science Lab
Affiliation: Undergraduate Research Assistants under Professor Veeravalli
Special Thanks to: Neeraj Venkatesan, UIUC M.S. EE graduate

*/

#define DEBUG 1

#include "air-quality.h" /* All includes, structure definitions, and macros are in this file- refer to it */

/* Function prototypes. More detailed descriptions follow in the implementations below */

void posToAddress(rimeaddr_t *out, unsigned int row, unsigned int col);
void addressToPos(rimeaddr_t in, unsigned int *row, unsigned int *col);
bool is_neighbor(node_t *one, node_t *two);
void generate_grid(node_t **topleft, struct memb *block, unsigned int n);
void free_grid(node_t *head, struct memb *block);
node_t* getNode(unsigned char addr, node_t *head);
void comms_close(struct broadcast_conn *one, struct unicast_conn *two);


MEMB(node_grid, node_t, NUM_MOTES); /* Declare memory block for an NxN grid */
LIST(node_list); /* Declare a Contiki list of node structures */

PROCESS(main_process, "main"); /* Declare main process */
PROCESS(wait_process, "Wait process"); /* Declare wait process */
AUTOSTART_PROCESSES(&main_process); /* Main process will start upon module load */

/* Set up communications */

static void unicast_recv(struct unicast_conn *c, const rimeaddr_t *from) /* Executes if unicast is received */
{
	static data_message_t message; /* Temporary variable to store received data */

	packetbuf_copyto(&message); /* Store contents of packet buffer */

	printf("Message received from %d.%d:\n", from->u8[0], from->u8[1]); /* Print the message */
	printf("The message key is %d, the temperature value is %d, and the humidity value is %d\n", message.key, message.temp, message.humid);
}

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)/* Do nothing if broadcast is received */
{
}

/* This is where we specify that unicast_recv should be called when a unicast is received.
The unicast_call structure is passed as a pointer to unicast_open in the main_process */

static const struct unicast_callbacks unicast_call = {unicast_recv};
static const struct broadcast_callbacks broadcast_call = {broadcast_recv}; /* Necessary to open a bc channel */

static struct unicast_conn uc;
static struct broadcast_conn bc; /* Used only to send messages to sniffer node */

PROCESS_THREAD(main_process, ev, data) /* protothread for main process */
{
	leds_off(LEDS_ALL); /* turn off LEDs */

	SENSORS_ACTIVATE(sht11_sensor); /* turn on the temperature/humidity sensor */

	/* Declare variables */

	static struct etimer et;
	static struct etimer periodic;
	static data_message_t out;
	out.address = NODE_ID; /* Initialize fields of out */
	out.key = M_KEY;
	rimeaddr_t addr;
	node_t *topleft; /* Pointer to top-left node in the grid */
	node_t *cur; /* Pointer to node on which this program is running */
	node_t *temp; /* Temporary node used in sending unicast */

	PROCESS_EXITHANDLER(comms_close(&bc, &uc)); /* Close the unicast upon exiting */
	PROCESS_BEGIN(); /* Begin process */

	/* Don't start data collection until user button is pressed */

	SENSORS_ACTIVATE(button_sensor);
	PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event) && (data == &button_sensor));

	generate_grid(&topleft, &node_grid, MAX_ROWS); /* First, generate the grid of nodes */

	unicast_open(&uc, COMM_CHANNEL, &unicast_call); /* open unicast on COMM_CHANNEL */
	broadcast_open(&bc, SNIFFER_CHANNEL, &broadcast_call); /* open broadcast for sniffer communication */

	/* Start wait process.  This will end the main process when a button sensor event is posted */
	process_start(&wait_process, NULL);

	while(1) /* infinite loop */
	{
		etimer_set(&periodic, CLOCK_SECOND*PERIOD); /* collect data every two seconds */
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic));

		/* Poll sensor */

		out.temp = sht11_sensor.value(SHT11_SENSOR_TEMP);
		out.humid = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

		packetbuf_copyfrom(&out, sizeof(out));
		broadcast_send(&bc); /* Broadcast to sniffer node */

		packetbuf_clear();

		packetbuf_copyfrom(&out, sizeof(out)); /* Encapsulate data from out into a packet to be sent */

		cur = getNode(NODE_ID, list_head(node_list)); /* Get a pointer to the node on which this program is 									running */

		if(cur == NULL) /* If we couldn't get a pointer to the node */
		{
			printf("Error! The node's address isn't what we thought it was.\n");
			exit(-1);
		}

		temp = list_head(node_list);

		while(temp != NULL) /* Send a unicast message to neighboring nodes */
		{
			if(is_neighbor(temp, cur))
			{
				unicast_send(&uc, &(temp->addr));
				
				#if DEBUG
					printf("This node has address %d\n", NODE_ID);
					printf("UC sent to %d\n", temp->addr.u8[0]);
				#endif
			}

			temp = list_item_next(temp);
		}


		/* Blink red LEDs to indicate that we sent a message */

		leds_on(LEDS_RED);
    
	  	etimer_set(&et, CLOCK_SECOND / 8 );
	  	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
	  	leds_off(LEDS_RED);

		etimer_reset(&periodic);
		packetbuf_clear();
	}

	SENSORS_DEACTIVATE(button_sensor); /* Deactivate sensors when we're done */
	SENSORS_DEACTIVATE(sht11_sensor);

	free_grid(list_head(node_list), &node_grid); /* Free the grid of nodes */

	PROCESS_END(); /* End process */
}

PROCESS_THREAD(wait_process, ev, data)
{
	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor); /* activate button sensor */
	
	/* Wait and return control to main_process until button sensor is pressed */
	PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event) && (data == &button_sensor));

	SENSORS_DEACTIVATE(button_sensor); /* Deactivate button sensor */
	SENSORS_DEACTIVATE(sht11_sensor); /* Deactivate sht_11 sensor */

	free_grid(list_head(node_list), &node_grid); /* Free the grid of nodes */

	process_exit(&main_process); /* End main process once button sensor is pressed */

	PROCESS_END();
}

/* Takes a node address given by row and column, and converts it into a rime address. */

void posToAddress(rimeaddr_t *out, unsigned int row, unsigned int col)
{
	out->u8[1] = 0; /* Upper eight bits of the rime address are not needed */
	out->u8[0] = START_ID + MAX_COLS*row + col; /* Performs conversion based on grid layout, defined in 								header file. */
}

/* Takes a rime address and converts it into (row, column) representation. Note: each of row, col
will lie in the range [0, MAX) where in this case MAX = MAX_ROWS = MAX_COLS */

void addressToPos(rimeaddr_t in, unsigned int *row, unsigned int *col)
{
	*col = (in.u8[0] - START_ID) % MAX_COLS; /* Subtract START_ID to reference the node using a number 								between 0 and 8 */
	*row = (in.u8[0] - START_ID) /MAX_COLS; /* Truncation ensures that we will get the correct row */
}

/* Takes pointers to two node_t structures and determines whether the two are neighbors */

bool is_neighbor(node_t *one, node_t *two)
{
	if((one->right == two) || (one->left == two) || (one->top == two) || (one->bottom == two))
		return true; /* return true if two is adjacent to one in the grid formation */

	else
		return false; /* return false otherwise */
}

/* Generates NxN grid of nodes with appropriate connections. Memory will be allocated from the block passed to the function.  Output (pointer to top left of the grid) is in the double node_t pointer topleft. */

void generate_grid(node_t **topleft, struct memb *block, unsigned int n)
{
	if(!n) /* Return in case of n = 0 (there's no grid to create) */
		return;

	node_t *temp, *temp1, *temp2, *temp3; /* Used to allocate memory and fix pointers */
	unsigned short i, j; /* for loop counters */

	for(i = 0; i < n; i++)
	{
		for(j = 0; j < n; j++)
		{
			temp = (node_t*) memb_alloc(block); /* Allocate memory for a node */

			if(temp == NULL)
			{
				printf("Could not allocate memory for a node. Exiting...\n");
				exit(-1);
			}

			posToAddress(&(temp->addr), i, j); /* Assign rime address to the node */

			if(j)
			{
				temp1->right = temp; /* Fix left/right pointers */
				temp->left = temp1;
			}

			if(i)
			{
				temp3->bottom = temp; /* Fix top/bottom pointers */
				temp->top = temp3;
				temp3 = temp3->right; /* proceed to next column */
			}

			/* Set appropriate null pointers for nodes on the border of the grid */
			
			if(!i)
				temp->top = NULL;

			if(i == (n-1))
				temp->bottom = NULL;

			if(!j)
			{
				temp->left = NULL;
				temp2 = temp; /* temp2 holds the node at the beginning of each row (useful for 							setting top/bottom pointers) */
			}

			if(j == (n-1))
				temp->right = NULL;

			if((i == 0) && (j == 0)) /* output a pointer to the top-left node */
				*topleft = temp;

			list_add(node_list, temp); /* Add the node to a linked list. This makes it easier to find an 								element in the grid */

			#if DEBUG

				printf("Allocated memory for node %d\n", temp->addr.u8[0]);

			#endif

			temp1 = temp;
		}

		temp3 = temp2;
	}

}

/* The following function frees a previously created grid given a pointer to the head of the linked list of nodes and the memory block from which the grid was created (size isn't necessary) */

void free_grid(node_t *head, struct memb *block)
{
	node_t *temp = head;
	node_t *temp1;

	while(temp != NULL) /* This loop deallocates memory blocks until all have been freed */
	{
		temp1 = temp->next; /* Store a copy of the next node in the list */

		if(memb_free(block, temp) == -1) /* free the node */
		{
			printf("Grid could not be freed; node pointer is invalid. Exiting now\n");
			exit(-1); /* print an error message and exit if node couldn't be freed */
		}

		temp = temp1; /* proceed to the next node */
	}
}

/* The following function returns a pointer to the node associated with a particular rime address, given a pointer to the head of the linked list of nodes.  Returns NULL if addr isn't found. */

node_t* getNode(unsigned char addr, node_t *head)
{
	node_t *temp = head;

	while(temp != NULL) /* Traverse the grid, looking for a node whose address matches addr */
	{
		if(temp->addr.u8[0] == addr) /* Return a pointer to the correct node */
			return temp;

		temp = list_item_next(temp); /* Proceed to the next node in the list */
	}

	return NULL;
}

/* Closes communications; called upon conclusion of the main process */

void comms_close(struct broadcast_conn *one, struct unicast_conn *two)
{
	broadcast_close(one);
	unicast_close(two);
}




