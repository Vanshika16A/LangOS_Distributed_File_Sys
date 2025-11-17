#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Universal System Error Codes
#define ERROR_PREFIX "ERROR"

// Standard format: ERROR;CODE;Message
// Example: ERROR;404;File not found.

#define ERR_UNKNOWN_COMMAND 400
#define ERR_PERMISSION_DENIED 403
#define ERR_FILE_NOT_FOUND 404
#define ERR_FILE_EXISTS 409
#define ERR_NO_SS_AVAILABLE 503
#define ERR_SS_FAILURE 504
#define ERR_INVALID_ARGS 422
#define ERR_NOT_OWNER 401
#define ERR_INVALID_INPUT     106
#define ERR_SERVER_MISC       107
#define ERR_SS_UNREACHABLE    108
#define  ERR_USER_NOT_FOUND    105


#endif // ERROR_CODES_H