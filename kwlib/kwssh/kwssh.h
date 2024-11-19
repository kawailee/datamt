#ifndef __KWSSH_H__
#define __KWSSH_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "libssh/libssh.h"
#include "libssh/sftp.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <ctype.h>
#include "KwMacro.h"

#define __SFTP_SUBSYSTEM__

//	https://www.redhat.com/en/blog/openssh-scp-deprecation-rhel-9-what-you-need-know
//	#define __SCP_SUBSYSTEM__
//	#define __KWSSH_USE_RSA__

//	OpenSSL 3.0
//	https://www.nist.gov/standardsgov/compliance-faqs-federal-information-processing-standards-fips

//
//	scp is not secured, going to switch to sftp subsystem
//
//	https://api.libssh.org/stable/libssh_tutor_sftp.html
//

int32_t	verify_knownhost(ssh_session s);
int32_t authenticate_kbdint(ssh_session s, const char *pw);
int32_t remote_session_execute(ssh_session s, const char *cmdstr);
int32_t split_path(char * dir, int32_t dir_size, char * file, int32_t file_size, const char *path);

#ifdef __SCP_SUBSYSTEM__
int32_t	scp_session_write(ssh_session s, const char *remote_path, const char *local_path);
int32_t	scp_receive(ssh_session s, ssh_scp scp, int32_t fd);
int32_t	scp_session_read(ssh_session s, const char *remote_path, const char *local_path);
int32_t	scp_write(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path);
int32_t	scp_read(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path);
#endif

#ifdef __SFTP_SUBSYSTEM__
int32_t	sftp_session_write(ssh_session s, const char *remote_path, const char *local_path);
int32_t	sftp_session_read(ssh_session s, const char *remote_path, const char *local_path);

int32_t	sftp_send(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path);
int32_t	sftp_receive(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path);
#endif

int32_t	read_linux_primary_password(char *pwd, size_t pwd_size);
int32_t	read_linux_password(char *pwd, size_t pwd_size, const char *prikey_path, const char *lnxkey_path);
int32_t	remote_execute(const char *host, const int32_t port, const char *un, const char *pw, const char *cmdstr);

#endif