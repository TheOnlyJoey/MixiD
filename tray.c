// System tray icon for MixiD, speaking the StatusNotifierItem protocol
// (org.kde.StatusNotifierItem) directly over D-Bus via sd-bus, so no GUI
// toolkit dependency is needed. Works out of the box on KDE and on anything
// else with a StatusNotifier tray; when no tray watcher is running,
// tray_init() reports failure and MixiD keeps its plain quit-on-close
// behaviour. Clicking the icon requests a show/hide toggle and right
// clicking opens the small com.canonical.dbusmenu menu implemented below;
// both surface through tray_pump().
#include "tray.h"

#ifdef HAVE_TRAY
#ifdef HAVE_BASU
#include <basu/sd-bus.h>
#else
#include <systemd/sd-bus.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ICON_SIZE 22
#define MENU_PATH "/MenuBar"

// dbusmenu item ids (0 is reserved for the root)
#define ITEM_TOGGLE      1
#define ITEM_QUIT        2
#define ITEM_SEP_TOP     3
#define ITEM_SEP_BOTTOM  4
#define ITEM_MASTER_BASE 10 // .. 10 + TRAY_MASTER_COUNT - 1

static sd_bus *bus = NULL;
static char busname[64];
static int pending = TRAY_NONE;
static uint8_t icon[ICON_SIZE * ICON_SIZE * 4]; // ARGB, network byte order

static void icon_rect(int x0, int y0, int w, int h, uint32_t argb)
{
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      uint8_t *p = &icon[(y * ICON_SIZE + x) * 4];
      p[0] = argb >> 24; p[1] = argb >> 16; p[2] = argb >> 8; p[3] = argb;
    }
}

// three little faders, matching what MixiD is about
static void icon_build(void)
{
  memset(icon, 0, sizeof(icon));
  const int track_x[3] = {4, 10, 16};
  const int cap_y[3]   = {4, 12, 8};
  for (int i = 0; i < 3; i++) {
    icon_rect(track_x[i], 2, 2, 18, 0xC0FFFFFF);
    icon_rect(track_x[i] - 1, cap_y[i], 4, 5, 0xFFFFFFFF);
  }
}

//----------------------------------------------------------------------------
// org.kde.StatusNotifierItem
//----------------------------------------------------------------------------

static int prop_string(sd_bus *b, const char *path, const char *iface,
                       const char *prop, sd_bus_message *reply, void *userdata,
                       sd_bus_error *error)
{
  const char *val = "";
  if (strcmp(prop, "Category") == 0) val = "ApplicationStatus";
  else if (strcmp(prop, "Id") == 0) val = "MixiD";
  else if (strcmp(prop, "Title") == 0) val = "MixiD";
  else if (strcmp(prop, "Status") == 0) val = "Active";
  return sd_bus_message_append(reply, "s", val);
}

static int prop_false(sd_bus *b, const char *path, const char *iface,
                      const char *prop, sd_bus_message *reply, void *userdata,
                      sd_bus_error *error)
{
  return sd_bus_message_append(reply, "b", 0);
}

static int prop_menu(sd_bus *b, const char *path, const char *iface,
                     const char *prop, sd_bus_message *reply, void *userdata,
                     sd_bus_error *error)
{
  return sd_bus_message_append(reply, "o", MENU_PATH);
}

static int prop_pixmap(sd_bus *b, const char *path, const char *iface,
                       const char *prop, sd_bus_message *reply, void *userdata,
                       sd_bus_error *error)
{
  int r = sd_bus_message_open_container(reply, 'a', "(iiay)");
  if (r < 0) return r;
  r = sd_bus_message_open_container(reply, 'r', "iiay");
  if (r < 0) return r;
  r = sd_bus_message_append(reply, "ii", ICON_SIZE, ICON_SIZE);
  if (r < 0) return r;
  r = sd_bus_message_append_array(reply, 'y', icon, sizeof(icon));
  if (r < 0) return r;
  r = sd_bus_message_close_container(reply);
  if (r < 0) return r;
  return sd_bus_message_close_container(reply);
}

static int method_click(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  pending = TRAY_TOGGLE;
  return sd_bus_reply_method_return(m, NULL);
}

static int method_ignore(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  return sd_bus_reply_method_return(m, NULL);
}

static const sd_bus_vtable item_vtable[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_PROPERTY("Category", "s", prop_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("Id", "s", prop_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("Title", "s", prop_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("Status", "s", prop_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("IconName", "s", prop_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("IconPixmap", "a(iiay)", prop_pixmap, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("ItemIsMenu", "b", prop_false, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("Menu", "o", prop_menu, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_METHOD("Activate", "ii", "", method_click, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("SecondaryActivate", "ii", "", method_click, SD_BUS_VTABLE_UNPRIVILEGED),
  // right click is served by the dbusmenu at MENU_PATH
  SD_BUS_METHOD("ContextMenu", "ii", "", method_ignore, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Scroll", "is", "", method_ignore, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_VTABLE_END
};

//----------------------------------------------------------------------------
// com.canonical.dbusmenu
//----------------------------------------------------------------------------

// master is the index into the monitor toggles when the item is a checkmark,
// -1 for a plain item
struct menu_entry { int32_t id; const char *label; int separator; int master; };

static const struct menu_entry menu[] = {
  {ITEM_TOGGLE,          "Show / Hide", 0, -1},
  {ITEM_SEP_TOP,         NULL,          1, -1},
  {ITEM_MASTER_BASE + 0, "Dim",         0,  0},
  {ITEM_MASTER_BASE + 1, "Alt",         0,  1},
  {ITEM_MASTER_BASE + 2, "Talk",        0,  2},
  {ITEM_MASTER_BASE + 3, "Phase",       0,  3},
  {ITEM_MASTER_BASE + 4, "Mono",        0,  4},
  {ITEM_SEP_BOTTOM,      NULL,          1, -1},
  {ITEM_QUIT,            "Quit",        0, -1},
};
#define MENU_COUNT ((int)(sizeof(menu) / sizeof(menu[0])))

static int master_state[TRAY_MASTER_COUNT];

static int append_prop_s(sd_bus_message *m, const char *key, const char *val)
{
  int r = sd_bus_message_open_container(m, 'e', "sv");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "s", key);
  if (r < 0) return r;
  r = sd_bus_message_open_container(m, 'v', "s");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "s", val);
  if (r < 0) return r;
  r = sd_bus_message_close_container(m);
  if (r < 0) return r;
  return sd_bus_message_close_container(m);
}

static int append_prop_b(sd_bus_message *m, const char *key, int val)
{
  int r = sd_bus_message_open_container(m, 'e', "sv");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "s", key);
  if (r < 0) return r;
  r = sd_bus_message_open_container(m, 'v', "b");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "b", val);
  if (r < 0) return r;
  r = sd_bus_message_close_container(m);
  if (r < 0) return r;
  return sd_bus_message_close_container(m);
}

static int append_prop_i(sd_bus_message *m, const char *key, int32_t val)
{
  int r = sd_bus_message_open_container(m, 'e', "sv");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "s", key);
  if (r < 0) return r;
  r = sd_bus_message_open_container(m, 'v', "i");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "i", val);
  if (r < 0) return r;
  r = sd_bus_message_close_container(m);
  if (r < 0) return r;
  return sd_bus_message_close_container(m);
}

// the a{sv} property dict of a single item
static int append_item_props(sd_bus_message *m, const struct menu_entry *e)
{
  int r = sd_bus_message_open_container(m, 'a', "{sv}");
  if (r < 0) return r;
  if (e->separator) {
    r = append_prop_s(m, "type", "separator");
    if (r < 0) return r;
  }
  else {
    r = append_prop_s(m, "label", e->label);
    if (r < 0) return r;
    r = append_prop_b(m, "enabled", 1);
    if (r < 0) return r;
    r = append_prop_b(m, "visible", 1);
    if (r < 0) return r;
    if (e->master >= 0) {
      r = append_prop_s(m, "toggle-type", "checkmark");
      if (r < 0) return r;
      r = append_prop_i(m, "toggle-state", master_state[e->master] ? 1 : 0);
      if (r < 0) return r;
    }
  }
  return sd_bus_message_close_container(m);
}

// one (ia{sv}av) child, wrapped in the variant the parent's av expects
static int append_child(sd_bus_message *m, const struct menu_entry *e)
{
  int r = sd_bus_message_open_container(m, 'v', "(ia{sv}av)");
  if (r < 0) return r;
  r = sd_bus_message_open_container(m, 'r', "ia{sv}av");
  if (r < 0) return r;
  r = sd_bus_message_append(m, "i", e->id);
  if (r < 0) return r;
  r = append_item_props(m, e);
  if (r < 0) return r;
  r = sd_bus_message_open_container(m, 'a', "v"); // leaf, no children
  if (r < 0) return r;
  r = sd_bus_message_close_container(m);
  if (r < 0) return r;
  r = sd_bus_message_close_container(m);
  if (r < 0) return r;
  return sd_bus_message_close_container(m);
}

static int menu_get_layout(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  int32_t parent = 0, depth = -1;
  int r = sd_bus_message_read(m, "ii", &parent, &depth);
  if (r < 0) return r;
  r = sd_bus_message_skip(m, "as"); // we always send every property
  if (r < 0) return r;

  sd_bus_message *reply = NULL;
  r = sd_bus_message_new_method_return(m, &reply);
  if (r < 0) return r;

  r = sd_bus_message_append(reply, "u", 1u); // layout revision, never changes
  if (r < 0) goto done;
  r = sd_bus_message_open_container(reply, 'r', "ia{sv}av");
  if (r < 0) goto done;
  r = sd_bus_message_append(reply, "i", parent);
  if (r < 0) goto done;
  r = sd_bus_message_open_container(reply, 'a', "{sv}");
  if (r < 0) goto done;
  r = append_prop_s(reply, "children-display", "submenu");
  if (r < 0) goto done;
  r = sd_bus_message_close_container(reply);
  if (r < 0) goto done;
  r = sd_bus_message_open_container(reply, 'a', "v");
  if (r < 0) goto done;
  if (parent == 0 && depth != 0) {
    for (int i = 0; i < MENU_COUNT; i++) {
      r = append_child(reply, &menu[i]);
      if (r < 0) goto done;
    }
  }
  r = sd_bus_message_close_container(reply);
  if (r < 0) goto done;
  r = sd_bus_message_close_container(reply);
  if (r < 0) goto done;

  r = sd_bus_send(NULL, reply, NULL);

done:
  sd_bus_message_unref(reply);
  return r;
}

static int menu_get_group_properties(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  // the requested id/property filters are ignored, the menu is three items
  int r = sd_bus_message_skip(m, "aias");
  if (r < 0) return r;

  sd_bus_message *reply = NULL;
  r = sd_bus_message_new_method_return(m, &reply);
  if (r < 0) return r;

  r = sd_bus_message_open_container(reply, 'a', "(ia{sv})");
  if (r < 0) goto done;
  for (int i = 0; i < MENU_COUNT; i++) {
    r = sd_bus_message_open_container(reply, 'r', "ia{sv}");
    if (r < 0) goto done;
    r = sd_bus_message_append(reply, "i", menu[i].id);
    if (r < 0) goto done;
    r = append_item_props(reply, &menu[i]);
    if (r < 0) goto done;
    r = sd_bus_message_close_container(reply);
    if (r < 0) goto done;
  }
  r = sd_bus_message_close_container(reply);
  if (r < 0) goto done;

  r = sd_bus_send(NULL, reply, NULL);

done:
  sd_bus_message_unref(reply);
  return r;
}

static int menu_get_property(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  int32_t id;
  const char *name;
  int r = sd_bus_message_read(m, "is", &id, &name);
  if (r < 0) return r;

  for (int i = 0; i < MENU_COUNT; i++) {
    if (menu[i].id != id)
      continue;
    if (strcmp(name, "label") == 0 && menu[i].label)
      return sd_bus_reply_method_return(m, "v", "s", menu[i].label);
    if (strcmp(name, "type") == 0)
      return sd_bus_reply_method_return(m, "v", "s", menu[i].separator ? "separator" : "standard");
    if (strcmp(name, "enabled") == 0 || strcmp(name, "visible") == 0)
      return sd_bus_reply_method_return(m, "v", "b", 1);
    if (strcmp(name, "toggle-type") == 0 && menu[i].master >= 0)
      return sd_bus_reply_method_return(m, "v", "s", "checkmark");
    if (strcmp(name, "toggle-state") == 0 && menu[i].master >= 0)
      return sd_bus_reply_method_return(m, "v", "i", master_state[menu[i].master] ? 1 : 0);
  }
  return sd_bus_reply_method_return(m, "v", "s", "");
}

static void menu_activate(int32_t id)
{
  if (id == ITEM_TOGGLE)
    pending = TRAY_TOGGLE;
  else if (id == ITEM_QUIT)
    pending = TRAY_QUIT;
  else if (id >= ITEM_MASTER_BASE && id < ITEM_MASTER_BASE + TRAY_MASTER_COUNT)
    pending = TRAY_MASTER + (id - ITEM_MASTER_BASE);
}

static int menu_event(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  int32_t id;
  const char *event;
  int r = sd_bus_message_read(m, "is", &id, &event);
  if (r < 0) return r;
  r = sd_bus_message_skip(m, "v");
  if (r < 0) return r;
  r = sd_bus_message_skip(m, "u");
  if (r < 0) return r;
  if (strcmp(event, "clicked") == 0)
    menu_activate(id);
  return sd_bus_reply_method_return(m, NULL);
}

static int menu_event_group(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  int r = sd_bus_message_enter_container(m, 'a', "(isvu)");
  if (r < 0) return r;
  while ((r = sd_bus_message_enter_container(m, 'r', "isvu")) > 0) {
    int32_t id;
    const char *event;
    if (sd_bus_message_read(m, "is", &id, &event) >= 0 &&
        sd_bus_message_skip(m, "v") >= 0 &&
        sd_bus_message_skip(m, "u") >= 0 &&
        strcmp(event, "clicked") == 0)
      menu_activate(id);
    r = sd_bus_message_exit_container(m);
    if (r < 0) return r;
  }
  if (r < 0) return r;
  r = sd_bus_message_exit_container(m);
  if (r < 0) return r;
  return sd_bus_reply_method_return(m, "ai", 0);
}

static int menu_about_to_show(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  return sd_bus_reply_method_return(m, "b", 0); // layout is static
}

static int menu_about_to_show_group(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
  return sd_bus_reply_method_return(m, "aiai", 0, 0);
}

static int prop_menu_version(sd_bus *b, const char *path, const char *iface,
                             const char *prop, sd_bus_message *reply, void *userdata,
                             sd_bus_error *error)
{
  return sd_bus_message_append(reply, "u", 3u);
}

static int prop_menu_string(sd_bus *b, const char *path, const char *iface,
                            const char *prop, sd_bus_message *reply, void *userdata,
                            sd_bus_error *error)
{
  return sd_bus_message_append(reply, "s",
                               strcmp(prop, "TextDirection") == 0 ? "ltr" : "normal");
}

static int prop_menu_icon_path(sd_bus *b, const char *path, const char *iface,
                               const char *prop, sd_bus_message *reply, void *userdata,
                               sd_bus_error *error)
{
  return sd_bus_message_append(reply, "as", 0);
}

static const sd_bus_vtable menu_vtable[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_PROPERTY("Version", "u", prop_menu_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("TextDirection", "s", prop_menu_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("Status", "s", prop_menu_string, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY("IconThemePath", "as", prop_menu_icon_path, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", menu_get_layout, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", menu_get_group_properties, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("GetProperty", "is", "v", menu_get_property, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Event", "isvu", "", menu_event, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", menu_event_group, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("AboutToShow", "i", "b", menu_about_to_show, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("AboutToShowGroup", "ai", "aiai", menu_about_to_show_group, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0),
  SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
  SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),
  SD_BUS_VTABLE_END
};

//----------------------------------------------------------------------------

int tray_init(void)
{
  if (sd_bus_open_user(&bus) < 0)
    return 0;
  icon_build();
  snprintf(busname, sizeof(busname), "org.kde.StatusNotifierItem-%d-1", (int)getpid());
  if (sd_bus_add_object_vtable(bus, NULL, "/StatusNotifierItem",
                               "org.kde.StatusNotifierItem", item_vtable, NULL) < 0 ||
      sd_bus_add_object_vtable(bus, NULL, MENU_PATH,
                               "com.canonical.dbusmenu", menu_vtable, NULL) < 0 ||
      sd_bus_request_name(bus, busname, 0) < 0 ||
      sd_bus_call_method(bus, "org.kde.StatusNotifierWatcher",
                         "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                         "RegisterStatusNotifierItem", NULL, NULL, "s", busname) < 0) {
    // no tray watcher on this desktop, caller keeps quit-on-close behaviour
    sd_bus_unref(bus);
    bus = NULL;
    return 0;
  }
  return 1;
}

int tray_pump(void)
{
  if (!bus)
    return TRAY_NONE;
  while (sd_bus_process(bus, NULL) > 0);
  int p = pending;
  pending = TRAY_NONE;
  return p;
}

// Push a toggle's state into the menu, so a checkmark flipped in the window
// and one flipped from the tray always agree.
void tray_set_master(int index, int on)
{
  if (index < 0 || index >= TRAY_MASTER_COUNT)
    return;
  master_state[index] = on ? 1 : 0;
  if (!bus)
    return;

  const struct menu_entry *entry = NULL;
  for (int i = 0; i < MENU_COUNT; i++)
    if (menu[i].master == index)
      entry = &menu[i];
  if (!entry)
    return;

  sd_bus_message *sig = NULL;
  if (sd_bus_message_new_signal(bus, &sig, MENU_PATH, "com.canonical.dbusmenu",
                                "ItemsPropertiesUpdated") < 0)
    return;
  int r = sd_bus_message_open_container(sig, 'a', "(ia{sv})");
  if (r < 0) goto done;
  r = sd_bus_message_open_container(sig, 'r', "ia{sv}");
  if (r < 0) goto done;
  r = sd_bus_message_append(sig, "i", entry->id);
  if (r < 0) goto done;
  r = append_item_props(sig, entry);
  if (r < 0) goto done;
  r = sd_bus_message_close_container(sig);
  if (r < 0) goto done;
  r = sd_bus_message_close_container(sig);
  if (r < 0) goto done;
  r = sd_bus_message_open_container(sig, 'a', "(ias)"); // nothing removed
  if (r < 0) goto done;
  r = sd_bus_message_close_container(sig);
  if (r < 0) goto done;

  sd_bus_send(NULL, sig, NULL);

done:
  sd_bus_message_unref(sig);
}

void tray_shutdown(void)
{
  if (!bus)
    return;
  sd_bus_release_name(bus, busname);
  sd_bus_flush(bus);
  sd_bus_unref(bus);
  bus = NULL;
}

#else // !HAVE_TRAY

int tray_init(void) { return 0; }
int tray_pump(void) { return TRAY_NONE; }
void tray_set_master(int index, int on) { (void)index; (void)on; }
void tray_shutdown(void) {}

#endif
