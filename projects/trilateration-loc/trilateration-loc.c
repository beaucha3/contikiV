#include "trilateration-loc.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "trilateration-localization.h"

/*---------------------------------------------------------------------------*/
PROCESS(trilateration_loc_process, "Trilateration Localization Process");
AUTOSTART_PROCESSES(&trilateration_loc_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(trilateration_loc_process, ev, data)
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

	shell_trilateration_localization_init();

	PROCESS_END();
}
