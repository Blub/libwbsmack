#ifndef SMACK_H_
#define SMACK_H_

#include <sys/types.h>

/* Predefined labels */
#define SMACK_STAR "*"
#define SMACK_FLOOR "_"
#define SMACK_HAT "^"

#define SMACK_OACCESSLEN (sizeof("rwxa") - 1)
#define SMACK_ACCESSLEN (sizeof("rwxat") - 1)

/* The default label */
#define SMACK_DEFAULT SMACK_FLOOR

/* Control file locations */
#define SMACK_LOAD "/smack/load"
#define SMACK_CIPSO "/smack/cipso"
#define SMACK_ACCESS "/smack/access"

#define SMACK_TRANSITION_FILE "/etc/smack/transition"
#define SMACK_TRANSITION_DIR "/etc/smack/transition.d"

#define SMACK_XATTR "security.SMACK64"

#define SMACK_PROCSELFATTRCURRENT "/proc/self/attr/current"

#define SMACK_USERS "/etc/smack/usr"

/* Maximum size of label, including null terminator. */
#define SMACK_SIZE 24
#define SMACK_SIZE_STR "24"

struct smackuser {
	char *su_name; ///< The listed smack username.
	char *su_label; ///< The smack label.
};

struct smackentry {
	char *su_name; ///< The username
	char **su_labels; ///< A list of labels accessible via smackentryget
	size_t su_labelcount; ///< The number of labels stored in su_labels
	int  su_any; ///< 1 if the user can take on any label
	size_t _su_allocated;
};

/* This function was written quite horribly, and is now replaced
   by something more sane...
   For instance, it used getline on the provided buffer which may
   cause it to be reallocated...
int getsmackuser_r(const char *name, struct smackuser *su,
                   char *buf, size_t buflen, struct smackuser **result);
*/

/**
 * Get a smack user entry.
 * out must point to a valid smackuser structure writable by the caller.
 * If @buffer is NULL, su_name and su_label will be allocated and have
 * to be freed by the caller.
 * On success, 0 is returned and out is filled.
 * On error, -1 is returned and errno is set:
 * ENOSYS - No smack-user file was found.
 * ENOMEM - Not enough memory in the provided buffer, or failed to
 *          allocate memory if buffer was NULL.
 * ENOENT - No user entry was found.
 */
int getsmackuser_r(const char *username, struct smackuser *out,
                   char *buffer, size_t buflen);

/**
 * Get the users from a smack user entry.
 * Must be closed with closesmackentry().
 */
struct smackentry* opensmackentry(const char *username);

/**
 * Get a label from a smackentry.
 * Returns NULL if index >= number of label entries.
 */
const char *smackentryget(struct smackentry const * entry, size_t index);

/**
 * Test whether or not a label is listed.
 * returns 1 if the label is listed or the '*ANY' label was added.
 */
int smackentrycontains(struct smackentry const * entry, const char *label);

/**
 * Close an opened smackuser entry.
 */
void closesmackentry(struct smackentry *entry);


/**
 * Retrieve the smack label of the current process.
 *
 * Label will be stored in the buffer provided in @label
 * limited in length by @n.
 *
 * On error, returns -1 with the following codes in errno:
 *
 * ENOSYS - not operating under smack
 * ENOMEM - insufficient memory in LABEL
 */
int getsmack(char *label, size_t n);

/**
 * Set the SMACK label of the current process.
 *
 * Fails by returning -1, error code in errno.
 */
int setsmack(const char *label);

/**
 * Check if SMACK is enabled.
 *
 */
int smackenabled(void);

/* The following two functions will only be available
 * statically. NOT in the dynamic library, to avoid
 * LD_PRELOAD attacks.
 */

/**
 * Check if SMACK would allow access.
 *
 * This requires permission to read /smack/load.
 */
int smackaccess(const char *subject, const char *object, char *access);


/**
 * Check if a label-transition is allowed by /etc/transition.d/...
 * This does not include an execute-access check!
 * You generally want to additionally do smackaccess(sub, obj, "x")
 * as well.
 * NOTE: While some caching is performed, each function call will
 *       still check the mtime of all transition related config
 *       files, and if a file changed, the whole set of rules is
 *       reloaded.
 * NOTE: Do not rely on this function in a dynamically linked
 *       executable!
 */
int smackchecktrans(const char *subject, const char *object);

#endif /* !SMACK_H_ */
