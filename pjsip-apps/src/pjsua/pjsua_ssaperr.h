#ifndef _PJSUA_SSAPERR_H_
#define _PJSUA_SSAPERR_H_

#include <pj/errno.h>


#define PJSUA_SSAP_ERRNO_START (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE * 8)

#define SSAP_ENULL				(PJSUA_SSAP_ERRNO_START +0)
#define SSAP_EMEMLEAK			(PJSUA_SSAP_ERRNO_START +1)
#define SSAP_EFRAGMENTED	(PJSUA_SSAP_ERRNO_START +2)
#define SSAP_EMEMCORRUPT	(PJSUA_SSAP_ERRNO_START +3)

#define guard(res,desc) test_result(res, desc, __func__)
//	res;do{if(res!=PJ_SUCCESS){PJ_PERROR(3,(THIS_FILE,res,"Error in(%s): %s",__func__,desc))}while(0)

#define break_unless(res,desc) \
	if(test_result(res, desc, __func__) != PJ_SUCCESS) break

#define unless(cond) if(!cond)
#define untill(cond) while(!cond)

//res;if(res!=PJ_SUCCESS){PJ_PERROR(3,(THIS_FILE,res,"Error in(%s): %s",__func__,desc))}break

#define not_null(res,desc) test_not_null(res, desc, __func__)


void* test_not_null(void* r, const char* const msg, const char* const func);
pj_status_t test_result(const pj_status_t resut, const char* const msg, const char* const func);

#endif /* _PJSUA_SSAPERR_H_ */


