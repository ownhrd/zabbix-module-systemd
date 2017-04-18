#include "libzbxsystemd.h"

static int SYSTEMD_MODVER(AGENT_REQUEST*, AGENT_RESULT*);
static int SYSTEMD_UNIT(AGENT_REQUEST*, AGENT_RESULT*);
static int SYSTEMD_UNIT_DISCOVERY(AGENT_REQUEST*, AGENT_RESULT*);

ZBX_METRIC *zbx_module_item_list()
{
  static ZBX_METRIC keys[] =
  {
    { "systemd.modver",         0,              SYSTEMD_MODVER,         NULL },
    { "systemd.unit",           CF_HAVEPARAMS,  SYSTEMD_UNIT,           "network,activestate" },
    { "systemd.unit.discovery", 0,              SYSTEMD_UNIT_DISCOVERY, NULL },
    { NULL }
  };

  return keys;
}

int zbx_module_api_version() {
  return ZBX_MODULE_API_VERSION;
}

int zbx_module_init()
{
    zabbix_log(LOG_LEVEL_INFORMATION, "starting module %s", PACKAGE_STRING);
    systemd_connect();
    return ZBX_MODULE_OK;
}

int zbx_module_uninit()
{ 
  dbus_connection_unref(conn);
  return ZBX_MODULE_OK;
}

int timeout = 3000;
void zbx_module_item_timeout(int t)
{
    timeout = t * 1000;
    zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "set timeout to %ims", timeout);
}

static int SYSTEMD_MODVER(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    SET_STR_RESULT(result, strdup(PACKAGE_STRING ", compiled with Zabbix " ZABBIX_VERSION));
    return SYSINFO_RET_OK;
}

// systemd.unit[<unit_name>,<property=SubState>]
static int SYSTEMD_UNIT(AGENT_REQUEST *request, AGENT_RESULT *result)
{
  char            *unit, *property;
  char            path[1024], value[1024];
  DBusMessageIter *iter = NULL;
  int             res = SYSINFO_RET_FAIL;

  if (1 > request->nparam || 2 < request->nparam) {
    SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
    return SYSINFO_RET_FAIL;
  }
 
  unit = get_rparam(request, 0);
  property = get_rparam(request, 1);
  if (NULL == property || '\0' == *property) {
    property = "SubState";
  }

  if (FAIL == systemd_get_unit(path, sizeof(path), unit)) {
    SET_MSG_RESULT(result, strdup("unit not found"));
    return res;
  }

  if (0 == dbus_marshall_property(
    result,
    SYSTEMD_SERVICE,
    path,
    SYSTEMD_UNIT_INTERFACE,
    property
  )) {
    res = SYSINFO_RET_OK;
  }
  
  return res;
}

// systemd.unit.discovery[]
static int SYSTEMD_UNIT_DISCOVERY(AGENT_REQUEST *request, AGENT_RESULT *result)
{
  DBusMessage     *msg = NULL;
  DBusMessageIter args, arr, unit;
  struct zbx_json j; 
  DBusBasicValue  value;
  int             res = SYSINFO_RET_FAIL;
  int             type = 0, i = 0, n = 0;

  // send method call
  msg = dbus_message_new_method_call(
    SYSTEMD_SERVICE,
    SYSTEMD_ROOT_NODE,
    SYSTEMD_MANAGER_INTERFACE,
    "ListUnits");

  if (NULL == (msg = dbus_exchange_message(msg))) {
    SET_MSG_RESULT(result, strdup("failed to list units"));
    return res;
  }

  // check result message
  if (!dbus_message_iter_init(msg, &args)) {
    zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "no value returned");
    return res;
  }
  
  if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&args)) {
    zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "returned value is not an array");
    return res;
  }
  
  zbx_json_init(&j, ZBX_JSON_STAT_BUF_LEN);
  zbx_json_addarray(&j, ZBX_PROTO_TAG_DATA);

  // loop through returned units
  dbus_message_iter_recurse(&args, &arr);
  while ((type = dbus_message_iter_get_arg_type (&arr)) != DBUS_TYPE_INVALID) {
    dbus_message_iter_recurse(&arr, &unit);

    // loop through values
    i = 0;
    while((type = dbus_message_iter_get_arg_type(&unit)) != DBUS_TYPE_INVALID) {
      dbus_message_iter_get_basic(&unit, &value);

      switch (i) {
      case 0:
        zbx_json_addobject(&j, NULL);
        zbx_json_addstring(&j, "{#UNIT.NAME}", value.str, ZBX_JSON_TYPE_STRING);
        break;

      case 1:
        zbx_json_addstring(&j, "{#UNIT.DESCRIPTION}", value.str, ZBX_JSON_TYPE_STRING);
        break;

      case 2:
        zbx_json_addstring(&j, "{#UNIT.LOADSTATE}", value.str, ZBX_JSON_TYPE_STRING);
        break;

      case 3:
        zbx_json_addstring(&j, "{#UNIT.ACTIVESTATE}", value.str, ZBX_JSON_TYPE_STRING);
        break;
      
      case 4:
        zbx_json_addstring(&j, "{#UNIT.SUBSTATE}", value.str, ZBX_JSON_TYPE_STRING);
        break;
      
      case 6:
        zbx_json_addstring(&j, "{#UNIT.OBJECTPATH}", value.str, ZBX_JSON_TYPE_STRING);
        break;
      }

      dbus_message_iter_next (&unit);
      i++;
    }
    zbx_json_close(&j);
    dbus_message_iter_next(&arr);
  }

  dbus_message_unref(msg);
  zbx_json_close(&j);
  SET_STR_RESULT(result, strdup(j.buffer));
  zbx_json_free(&j);

  return SYSINFO_RET_OK;
}