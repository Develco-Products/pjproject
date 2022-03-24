#include "pjsua_bluetoothctl.h"

#include "pjsua_ssapmsg.h"

#define THIS_FILE           "pjsua_bluetoothctl"
#define BLUETOOTHCTL_PATH   "/usr/bin/bluetoothctl"
#define UCI_PATH            "/usr/bin/uci"
#define UCI_BT_ADDRESS_NODE "btaudiomanager.speaker.address"  // uci node.
#define BT_CONTROLLER_NODE  "production.linux.bdaddr"         // config node.

#define SYSCMD_TIMEOUT_ms (3000)
#define SYSCMD_OUTP_BUFFER (1024)

// global
// ssap_mem - global memory pool for pjsua-ssap.
// located in pjsua_app_legacy.c
extern pj_caching_pool ssap_mem;

//static const pj_str_t* system_cmd = NULL;
static const char* system_cmd = NULL;
static pj_pool_t* syscmd_pool = NULL;
static pj_thread_t* cmd_runner = NULL;
static pj_mutex_t* cmd_runner_block = NULL;             // sync for caller.
static pj_bool_t everything_is_alright = PJ_TRUE;        // for thread to signal early death.
static int system_ret = 0;                              // Used by cmd_runner. Do not access directly.

// set by calling function.
//static char* syscmd_outp_buffer;            // statically allocated in calling function.
//static pj_str_t* syscmd_outp_buffer;
static char syscmd_outp_buffer[SSAPMSG_PAYLOAD_BUFFER_MAX];
static pj_ssize_t syscmd_buffer_size;

// ssap_exec
// thread proc. 
int ssap_exec();

static pj_status_t run_cmd(const char* cmd, int* const sysret) {
  if(syscmd_pool != NULL) {
    PJ_LOG(3, (THIS_FILE, "Cannoct allocate memory for system command twice."));
    return PJ_EEXISTS;
  }

  const pj_ssize_t pool_size = PJ_THREAD_DESC_SIZE
      + 64; // est. mutex size.

  syscmd_pool = pj_pool_create(&ssap_mem.factory, "ssap_syscmd", pool_size, 32, NULL);
  if(syscmd_pool == NULL) {
    PJ_PERROR(3, (THIS_FILE, PJ_ENOMEM, "Could not allocate memory for running system command."));
    return PJ_ENOMEM;
  }

  pj_status_t res = pj_mutex_create(syscmd_pool, "cmd_runner_lock", PJ_MUTEX_SIMPLE, &cmd_runner_block);
  if(res != PJ_SUCCESS) {
    PJ_PERROR(3, (THIS_FILE, res, "Could not create cmd_runner mutex."));
    return res;
  }

  system_cmd = cmd;
  everything_is_alright = PJ_TRUE;

  res = pj_thread_create(syscmd_pool, "ssap_exec", (pj_thread_proc*) ssap_exec, NULL, PJ_THREAD_DEFAULT_STACK_SIZE, 0, &cmd_runner);
  if(res != PJ_SUCCESS) {
    PJ_PERROR(3, (THIS_FILE, res, "Could not create command runner thread."));
    return res;
  }

  pj_thread_sleep(50);
  int timer_count = 50;

  while(everything_is_alright && pj_mutex_is_locked(cmd_runner_block)) {
    if(timer_count >= SYSCMD_TIMEOUT_ms) {
      PJ_LOG(3, (THIS_FILE, "system command timed out."));
      everything_is_alright = PJ_FALSE;
    }
    else {
      pj_thread_sleep(100);
      timer_count += 100;
    }
  }

  /* Reset variables depending on how it went. */
  if(everything_is_alright) {
    pj_thread_join(cmd_runner);
  }
  else {
    pj_thread_destroy(cmd_runner);
    system_ret = PJ_ETIMEDOUT;
  }
  cmd_runner = NULL;

  pj_mutex_destroy(cmd_runner_block);
  cmd_runner_block = NULL;

  pj_pool_release(syscmd_pool);
  syscmd_pool = NULL;

  *sysret = system_ret;
  return PJ_SUCCESS;
}


// Delegated system commands to thread to avoid system hangs due to unresponsive software.
// As an example, try running bluetoothctl list on build server.
int ssap_exec() {
  pj_status_t res = pj_mutex_lock(cmd_runner_block);
  if(res != PJ_SUCCESS) {
    PJ_PERROR(4, (THIS_FILE, res, "Could not aquire mutex lock."));
    system_ret = res;
    return res;
  }
  
  PJ_LOG(5, (THIS_FILE, "ssap_exec: %s", system_cmd));

  FILE* proc = popen(system_cmd, "r");
  if(proc != NULL) {
    ssize_t size = 0;
#if 0
    char buf[128];
    size_t bite = (syscmd_buffer_size >= 128) 
        ? 128
        : syscmd_buffer_size;
#endif

    //PJ_LOG(3, (THIS_FILE, ","));
    // Clear content of receiving buffer.
    //pj_strcpy2(syscmd_outp_buffer, "");
    //strcpy(syscmd_outp_buffer, "");

    //PJ_LOG(3, (THIS_FILE, ","));
    //while(fgets(buf, bite, proc) != NULL || size >= syscmd_buffer_size) {
    while(fgets(syscmd_outp_buffer + size, syscmd_buffer_size, proc) != NULL || size >= syscmd_buffer_size) {
      //pj_strcat2(syscmd_outp_buffer, buf);
      //strcat(syscmd_outp_buffer + size, buf);
      //size += strlen(buf);
      size += strlen(syscmd_outp_buffer + size);
     // PJ_LOG(3, (THIS_FILE, "-"));
#if 0
      PJ_LOG(3, (THIS_FILE, ":%s", buf));
      bite = (syscmd_buffer_size - size >= 128) 
          ? 128
          : syscmd_buffer_size - size;
#endif
    }
    //PJ_LOG(3, (THIS_FILE, ","));
    pclose(proc);
    //PJ_LOG(3, (THIS_FILE, ","));

    //PJ_LOG(3, (THIS_FILE, "ssap_exec output (%dB):\n%s", pj_strlen(syscmd_outp_buffer), pj_strbuf(syscmd_outp_buffer)));
    PJ_LOG(5, (THIS_FILE, "ssap_exec output (%dB):\n%s", size, syscmd_outp_buffer));
  }
  else {
    //PJ_LOG(3, (THIS_FILE, "!"));
    system_ret = errno;
    PJ_PERROR(3, (THIS_FILE, system_ret, "Error trying to execute system command."));
    //PJ_LOG(3, (THIS_FILE, "!"));
    return system_ret;
  }
  //PJ_LOG(3, (THIS_FILE, "."));

  pj_mutex_unlock(cmd_runner_block);
  //PJ_LOG(3, (THIS_FILE, "."));
  return 0;
}

pj_status_t bluetoothctl_controller_status(char* device_status, pj_ssize_t buffer_size) {
  //PJ_LOG(3, (THIS_FILE, "%s", __func__));
  syscmd_buffer_size = buffer_size;

  char cmd[128];
  int sysret = 0;

  strcpy(cmd, "getcfg " BT_CONTROLLER_NODE);
  pj_status_t ret = run_cmd(cmd, &sysret);
  
  if(ret != PJ_SUCCESS || sysret != 0) {
    PJ_PERROR(3, (THIS_FILE, (ret != PJ_SUCCESS ? ret : sysret), "Failure executing system command."));
    if(ret == PJ_SUCCESS) {
      ret = PJ_EUNKNOWN;
    }

    return ret;
  }

  /* bt address is returned without seperating ':'s */
  strcpy(cmd, BLUETOOTHCTL_PATH " show ");
  int offset = strlen(cmd);
  for(int i=0; i<strlen(syscmd_outp_buffer); i+=2) {
    cmd[offset++] = syscmd_outp_buffer[i +0];
    cmd[offset++] = syscmd_outp_buffer[i +1];
    cmd[offset++] = ':';
  }
  // overwrite the last ':' with a terminating null.
  cmd[offset -1] = '\0';

  ret = run_cmd(cmd, &sysret);
  
  if(ret != PJ_SUCCESS || sysret != 0) {
    PJ_PERROR(3, (THIS_FILE, (ret != PJ_SUCCESS ? ret : sysret), "Failure executing system command."));
    if(ret == PJ_SUCCESS) {
      ret = PJ_EUNKNOWN;
    }

    return ret;
  }

  /* Copy reply back into device status. */
  strcpy(device_status, syscmd_outp_buffer);
  return PJ_SUCCESS;
}

pj_status_t bluetoothctl_device_status(char* device_status, pj_ssize_t buffer_size) {
  syscmd_buffer_size = buffer_size;

  char cmd[128];
  int sysret = 0;

  strcpy(cmd, UCI_PATH " get " UCI_BT_ADDRESS_NODE);
  pj_status_t ret = run_cmd(cmd, &sysret);

  if(ret != PJ_SUCCESS || sysret != 0) {
    PJ_PERROR(3, (THIS_FILE, (ret != PJ_SUCCESS ? ret : sysret), "Failure executing system command."));
    if(ret == PJ_SUCCESS) {
      ret = PJ_EUNKNOWN;
    }

    return ret;
  }

  strcpy(cmd, BLUETOOTHCTL_PATH " info ");
  strcat(cmd, syscmd_outp_buffer);

  ret = run_cmd(cmd, &sysret);
  if(ret != PJ_SUCCESS || sysret != 0) {
    PJ_PERROR(3, (THIS_FILE, (ret != PJ_SUCCESS ? ret : sysret), "Failure executing system command."));
    if(ret == PJ_SUCCESS) {
      ret = PJ_EUNKNOWN;
    }

    return ret;
  }

  /* Copy reply back into device status. */
  strcpy(device_status, syscmd_outp_buffer);
  return PJ_SUCCESS;
}

