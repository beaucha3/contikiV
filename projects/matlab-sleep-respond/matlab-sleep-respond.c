#include "matlab-sleep-respond.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "matlab-response-protocol.h"

/*---------------------------------------------------------------------------*/
PROCESS(matlab_sleep_respond_process, "Simple Response Protocol for Matlab");
AUTOSTART_PROCESSES(&matlab_sleep_respond_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(matlab_sleep_respond_process, ev, data)
{
	PROCESS_BEGIN();
	serial_shell_init();
	//shell_blink_init();
	//shell_reboot_init();
	shell_rime_init();
	//shell_rime_netcmd_init();
	shell_text_init();
	shell_time_init();
	shell_sky_init();
	//shell_collect_view_init();

	shell_global_sensor_init();
	shell_matlab_response_protocol_init();

	PROCESS_END();
}
