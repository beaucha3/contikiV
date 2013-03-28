#include "grad-desc-localization-test.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "grad-desc-localization.h"

/*---------------------------------------------------------------------------*/
PROCESS(grad_desc_localization_test_process, "Grad-Desc Localization Test Process");
AUTOSTART_PROCESSES(&grad_desc_localization_test_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(grad_desc_localization_test_process, ev, data)
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

	shell_grad_desc_localization_init();

	PROCESS_END();
}
