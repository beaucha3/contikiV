#include "APP HEADER FILE"
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h" // Include if you're using mesh transmission
#include "shell.h"
#include "shell-collect-view.h" // Do you need this?
#include "collect-view.h" // Or this?

#include "dev/leds.h" // Also include any requisite sensors
#include "dev/serial-line.h" // Need to look at what this does
#include <stdio.h> 
#include <string.h>

/*---------------------------------------------------------------------------*/
PROCESS(shell_example_process, "example");
SHELL_COMMAND(example_command,
              "example",
              "example: this is an example description",
              &shell_example_process);
/*---------------------------------------------------------------------------*/
/* Transmission method functions go up here, ie for broadcast, unicast, mesh, etc.
 * Try to keep everything transmission-related in this block, as it is not required
 * elsewhere in the program.
 */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_example_process, ev, data)
{
	static struct etimer etimer; // The etimer likes being ahead of the PROCESS macros
 
	PROCESS_EXITHANDLER(mesh_close(&mesh);) // What the process should do if sent process_exit()
	PROCESS_BEGIN();

	/* Your code here */

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void shell_command_list_init(void)
{
	mesh_open(&mesh, 132, &callbacks); // Open your connection HERE, not in the process
	shell_register_command(&example_command);
}
