/* This is a practice change detection "app" for the tmote sky.
 * Author: Dario Aranguiz
 */


#include "simple-detect.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "mean-shift.h"

/*---------------------------------------------------------------------------*/
PROCESS(simple_detect_process, "Simple Detect Process");
AUTOSTART_PROCESSES(&simple_detect_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(simple_detect_process, ev, data)
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
	
	// Here's where it differs from the normal collect-view program
	shell_mean_shift_init();

	PROCESS_END();
}
