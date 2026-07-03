#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __AUTHENTICATE_H__
#define __AUTHENTICAT_H__

#ifdef __cplusplus
extern "C" {
#endif

int  RtspAuthenticateDigest(char *pInputBuf,char *pServerUsername,char *pServerPassword);

#ifdef __cplusplus
}
#endif
#endif

