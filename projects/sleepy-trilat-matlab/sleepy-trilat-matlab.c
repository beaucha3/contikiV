#include "sleepy-trilat-matlab.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "sleepy-trilateration-matlab.h"

/*---------------------------------------------------------------------------*/
PROCESS(sleepy_trilateration_matlab_process, "Sleepy Trilateration Matlab Plugin");
AUTOSTART_PROCESSES(&sleepy_trilateration_matlab_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(sleepy_trilateration_matlab_process, ev, data)
{
	PROCESS_BEGIN();
	serial_shell_init();
	shell_blink_init();
	shell_reboot_init();
	shell_rime_init();
	shell_rime_netcmd_init();
	shell_text_init();
	shell_time_init();
	shell_sky_init();
	shell_collect_view_init();

	shell_global_sensor_init();
	shell_sleepy_trilateration_matlab_init();

	PROCESS_END();
}
