#include "net/rime.h"
#include "net/rime/mesh.h" // If you're using mesh

/*-----------------------------Broadcast-------------------------------------*/
/*---------------------------------------------------------------------------*/

// Init structure:
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast; 

// To send a message:
	packetbuf_copyfrom("Your message here", sizeof("Your message here"));
	broadcast_send(&broadcast);

// To open/close a connection:
	broadcast_open(&broadcast, 129, &broadcast_call);
	broadcast_close(&broadcast);



/*--------------------------------Mesh---------------------------------------*/
/*---------------------------------------------------------------------------*/

// Init structure:
static struct mesh_conn mesh;

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
  printf("Data received from %d.%d: %s (%d)\n",
         from->u8[0], from->u8[1],
         (char *)packetbuf_dataptr(), packetbuf_datalen());

//  packetbuf_copyfrom("Hop", strlen("Hop"));
//  mesh_send(&mesh, from);
}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};


// To send a message:
	rimaddr_t addr;
	packetbuf_copyfrom("Your message here", sizeof("Your message here"));
	addr.u8[0] = 62;  // Note: this refers to the node with address 62.41
	addr.u8[1] = 41; 
	mesh_send(&mesh, &addr);

// To open/close a connection:
	mesh_open(&mesh, 132, &callbacks);
	mesh_close(&mesh);

/*---------------------------------------------------------------------------*/
