#include <bps/virtualkeyboard.h>
#include <screen/screen.h>
#include <sqlite3.h>

/* Link-only probe. No Screen contexts or input events are created. */
int (*volatile provider_context_api)(screen_context_t *, int) =
    screen_create_context;
int (*volatile session_api)(screen_session_t *, screen_context_t, int) =
    screen_create_session_type;
int (*volatile inject_api)(screen_display_t, screen_event_t) =
    screen_inject_event;
int (*volatile send_api)(screen_context_t, screen_event_t, pid_t) =
    screen_send_event;
const char *(*volatile sqlite_api)(void) = sqlite3_libversion;

int main(void)
{
    return SCREEN_INPUT_PROVIDER_CONTEXT == 0 ||
           SCREEN_INPUT_MANAGER_CONTEXT == 0 ||
           SCREEN_WINDOW_MANAGER_CONTEXT == 0;
}
