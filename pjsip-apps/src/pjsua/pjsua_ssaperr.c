#include "pjsua_ssaperr.h"


void* test_not_null(void* r, const char* const msg, const char* const func) {
  if(res == NULL) {
    PJ_PERROR(3, (THIS_FILE, SSAP_ENULL, "Null-error in(%s): %s", func, desc));
  }
  return res;
}


pj_status_t test_result(const pj_status_t resut, const char* const msg, const char* const func) {
  if(res != PJ_SUCCESS) {
    PJ_PERROR(3, (THIS_FILE, res, "Null-error in(%s): %s", func, desc));
  }
  return res;
}
    

