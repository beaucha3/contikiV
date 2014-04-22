/* This is the gaussian CUSUM project's main source file.
 * Intended exclusively for use on the TelosB Sky modules.
 * Author: Kyle Harris
 */

#include "sleepy-cusum.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "sleepy-cusum-node.h"
#include "global-sensor.h"
#include "tool-sample-stats.h"

/*---------------------------------------------------------------------------*/
PROCESS(gaussian_cusum_process, "Gaussian CUSUM Process");
AUTOSTART_PROCESSES(&gaussian_cusum_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(gaussian_cusum_process, ev, data) {
	PROCESS_BEGIN();
	serial_shell_init();
	shell_blink_init();
	shell_reboot_init();
	shell_rime_init();
//	shell_rime_netcmd_init();
	shell_text_init();
	shell_time_init();
	shell_sky_init();
	shell_collect_view_init();

	// Here's where it differs from the normal collect-view program.	
	shell_sleepy_cusum_init();
	shell_global_sensor_init();
	shell_sample_stats_init();

	PROCESS_END();
}
