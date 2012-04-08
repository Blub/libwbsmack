#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <security/pam_ext.h>

#define PAM_SM_SESSION
#include <security/pam_modules.h>

#include "../src/smack.h"

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
                     int argc, const char **argv)
{
	return PAM_IGNORE;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
                    int argc, const char **argv)
{
	struct smackuser su;
        const char *user;
        int rc;

	rc = pam_get_item(pamh, PAM_USER, (const void**)&user);
	if (rc != PAM_SUCCESS || user == NULL || !*user) {
		pam_syslog(pamh, LOG_ERR, "Can't determine user\n");
		(void)setsmack(SMACK_FLOOR);
		return PAM_USER_UNKNOWN;
	}

	rc = getsmackuser_r(user, &su, NULL, 0);
	if (rc != 0) {
		pam_syslog(pamh, LOG_ERR,
		           "User %s does not have a smack label defined", user);
		return PAM_USER_UNKNOWN;
	}

	rc = setsmack(su.su_label);
	free(su.su_label);
	free(su.su_name);
	if (rc) {
		pam_syslog(pamh, LOG_ERR,
		           "Failed to set label %s for user %s",
		           su.su_label, user);
		return PAM_SYSTEM_ERR;
	}
	return PAM_SUCCESS;
}

#ifdef PAM_STATIC
struct pam_module _pam_wbsmack_modstruct = {
	"pam_wbsmack",
	NULL,
	NULL,
	NULL,
	pam_sm_open_session,
	pam_sm_close_session,
	NULL
};
#endif
