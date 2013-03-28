#include "test-project.h"
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "collect-view.h"
#include "test-app.h"

/*---------------------------------------------------------------------------*/
PROCESS(test_project_process, "Test Project Process");
AUTOSTART_PROCESSES(&test_project_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(test_project_process, ev, data)
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

	shell_test_app_init();

	PROCESS_END();
}
