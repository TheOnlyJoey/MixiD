#ifndef _H_TRAY_H_
#define _H_TRAY_H_

// StatusNotifierItem tray icon (see tray.c). All calls are safe no-ops when
// built without HAVE_TRAY or when the desktop has no tray watcher running.

#define TRAY_NONE   0
#define TRAY_TOGGLE 1 // icon clicked, or "Show / Hide" picked from its menu
#define TRAY_QUIT   2 // "Quit" picked from the tray menu
#define TRAY_MASTER 16 // TRAY_MASTER + n: master toggle n picked from the menu

#define TRAY_MASTER_COUNT 5 // Dim, Alt, Talk, Phase, Mono

#ifdef __cplusplus
extern "C" {
#endif

int tray_init(void);      // 1 if a tray icon is up, 0 if unavailable
int tray_pump(void);      // call once per frame; returns a TRAY_* request
void tray_set_master(int index, int on); // mirror a master toggle into the menu
void tray_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
