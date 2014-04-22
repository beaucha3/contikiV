#include "hello-world.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "say-hi.h"

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello World Process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(hello_world_process, ev, data)
{
	PROCESS_BEGIN();
	serial_shell_init();
	shell_blink_init();
	shell_reboot_init();
	shell_rime_init();
	shell_rime_netcmd_init();
	shell_powertrace_init();
	shell_text_init();
	shell_time_init();
	shell_sky_init();
	shell_collect_view_init();

//	shell_codeprop_init();	
	// Here's where it differs from the normal collect-view program
	shell_hello_world_init();

	PROCESS_END();
}
