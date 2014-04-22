#include "adaptive-cusum.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "de-adaptive-cusum.h"
#include "global-sensor.h"
#include "tool-sample-stats.h"

/*---------------------------------------------------------------------------*/
PROCESS(adaptive_cusum_process, "Adaptive CUSUM Process");
AUTOSTART_PROCESSES(&adaptive_cusum_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(adaptive_cusum_process, ev, data)
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

	shell_adaptive_cusum_init();
	shell_global_sensor_init();
	shell_sample_stats_init();

	PROCESS_END();
}
