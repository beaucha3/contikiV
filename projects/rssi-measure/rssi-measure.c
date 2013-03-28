#include "rssi-measure.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "rssi.h"

/*---------------------------------------------------------------------------*/
PROCESS(rssi_measure_process, "RSSI Measure Process");
AUTOSTART_PROCESSES(&rssi_measure_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(rssi_measure_process, ev, data)
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
	shell_rssi_measure_init();

	PROCESS_END();
}
