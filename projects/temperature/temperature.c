/* This is the gaussian CUSUM project's main source file.
 * Intended exclusively for use on the TelosB Sky modules.
 * Author: Kyle Harris
 */

#include "temperature.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "global-sensor.h"
#include "tool-sample-stats.h"
#include "test-change-detect.h"

/*---------------------------------------------------------------------------*/
PROCESS(temperature_process, "Temperature Localization Process");
AUTOSTART_PROCESSES(&temperature_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(temperature_process, ev, data) {
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

	// Here's where it differs from the normal collect-view program.	
	shell_global_sensor_init();
	shell_sample_stats_init();
	shell_test_change_detect_init();

	PROCESS_END();
}
