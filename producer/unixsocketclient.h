
#ifndef UNIXSOCKET_CLIENT_H
    #define UNIXSOCKET_CLIENT_H

#define CLI_PATH    "/var/tmp/"        /* +5 for pid = 14 chars */
#define CLI_PERM    S_IRWXU            /* rwx for user only */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

int cli_conn (const char* name, char project);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // UNIXSOCKET_CLIENT_H


