#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "kwssh.h"

int32_t	verify_knownhost(ssh_session s) {
	int32_t		rc = 0;
	enum ssh_known_hosts_e		state;
	unsigned char *				hash = NULL;
	ssh_key						srv_pubkey = NULL;

	size_t		hlen;
	char		buf[10];
	char		*hexa;

	rc = ssh_get_server_publickey(s, &srv_pubkey);
	if (rc != SSH_OK) {
		fprintf(stderr, "ssh_get_server_publickey failed.");
		return -1;
	}

	rc = ssh_get_publickey_hash(srv_pubkey, SSH_PUBLICKEY_HASH_SHA1, &hash, &hlen);
	ssh_key_free(srv_pubkey);
	if (rc != SSH_OK) {
		fprintf(stderr, "ssh_get_publickey_hash failed.");
		return rc;
	}

	state = ssh_session_is_known_server(s);
	switch(state) {

		case SSH_KNOWN_HOSTS_OK:
			break;

		case SSH_KNOWN_HOSTS_CHANGED:
			hexa = ssh_get_hexa(hash, hlen);
			fprintf(stderr, "Host key for server changed: it is now:\n");
			fprintf(stderr, "Public key hash: %s\n", hexa);
			fprintf(stderr, "For security reasons, connection will be stopped\n");
			ssh_string_free_char(hexa);
			ssh_clean_pubkey_hash(&hash);
			return -1;
			break;
			
		case SSH_KNOWN_HOSTS_OTHER:
			fprintf(stderr, "The host key for this server was not found but an other type of key exists.\n");
			fprintf(stderr, "An attacker might change the default server key to confuse your client into thinking the key does not exists.\n");
			ssh_clean_pubkey_hash(&hash);
			return -1;
			break;

		case SSH_KNOWN_HOSTS_NOT_FOUND:
			fprintf(stderr, "Could not find known host file.\n");
			fprintf(stderr, "The file will be automatically created.\n");
			// fallback to SSH_KNOWN_HOSTS_UNKNOWN

		case SSH_KNOWN_HOSTS_UNKNOWN:
			hexa = ssh_get_hexa(hash, hlen);
			fprintf(stderr, "The server is unknown. Batch mode will trust the host key automatically.\n");
			fprintf(stderr, "Public key hash: %s\n", hexa);
			ssh_string_free_char(hexa);
			ssh_clean_pubkey_hash(&hash);

			if (ssh_session_update_known_hosts(s) != SSH_OK) {
				fprintf(stderr, "Error %s\n", strerror(errno));
				ssh_clean_pubkey_hash(&hash);
				return -1;
			}
			break;

		default:
			fprintf(stderr, "Unknown server state %d", state);
			ssh_clean_pubkey_hash(&hash);
			return -1;
			break;
	}

	ssh_clean_pubkey_hash(&hash);
	return rc;
}

int32_t authenticate_kbdint(ssh_session s, const char *pw) {
	int32_t		rc = SSH_AUTH_SUCCESS;

	rc = ssh_userauth_kbdint(s, NULL, NULL);
	while (rc == SSH_AUTH_INFO) {
		const char *name, *instruction;
		int nprompts, iprompt;

		name = ssh_userauth_kbdint_getname(s);
		instruction = ssh_userauth_kbdint_getinstruction(s);
		nprompts = ssh_userauth_kbdint_getnprompts(s);

		if (strlen(name) > 0) {
			fprintf(stdout, "%s\n", name);
		}

		if (strlen(instruction) > 0) {
			fprintf(stdout, "%s\n", instruction);
		}

		for( iprompt = 0; iprompt < nprompts; iprompt++) {
			const char *prompt;
			char echo;

			prompt = ssh_userauth_kbdint_getprompt(s, iprompt, &echo);
			//	fprintf(stdout, "%02d:%s:0x%02X\n", iprompt, prompt, echo);
			//	fflush(stdout);

			if (echo) {
				char buffer[128], *ptr;
				fprintf(stdout, "%s", prompt);
				fflush(stdout);
				if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
					return SSH_AUTH_ERROR;
				}
				buffer[sizeof(buffer)-1] = '\0';
				if ((ptr = strchr(buffer, '\n')) != NULL) {
					*ptr = '\0';
				}
				if (ssh_userauth_kbdint_setanswer(s, iprompt, buffer) < 0) {
					fprintf(stderr, "ssh_userauth_kbdint_setanswer() failed: %s\n", ssh_get_error(s));
					return SSH_AUTH_ERROR;
				}
				memset(buffer, 0, strlen(buffer));
			} else {
				//	fprintf(stdout, "%02d:%s\n", iprompt, pw);
				//	fflush(stdout);

				if (ssh_userauth_kbdint_setanswer(s, iprompt, pw) < 0) {
					fprintf(stderr, "ssh_userauth_kbdint_setanswer() failed: %s\n", ssh_get_error(s));
					return SSH_AUTH_ERROR;
				}
			}
		}
		rc = ssh_userauth_kbdint(s, NULL, NULL);
	}
	return rc;
}

int32_t remote_session_execute(ssh_session s, const char *cmdstr) {
	int32_t rc = SSH_OK;
	ssh_channel ch;
	char buffer[2048];
	int32_t nbytes;

	if ((ch = ssh_channel_new(s)) == NULL) {
		fprintf(stderr, "ssh_channel_new() failed: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	if ((rc = ssh_channel_open_session(ch)) != SSH_OK) {
		fprintf(stderr, "ssh_channel_open_session() failed: %s\n", ssh_get_error(s));
		ssh_channel_free(ch);
		return SSH_ERROR;
	}

	if ((rc = ssh_channel_request_exec(ch, cmdstr)) != SSH_OK) {
		fprintf(stderr, "ssh_channel_request_exec() failed: %s\n", ssh_get_error(s));
		ssh_channel_close(ch);
		ssh_channel_free(ch);
		return SSH_ERROR;
	}

	while ((nbytes = ssh_channel_read(ch, buffer, sizeof(buffer), 0)) > 0 ) {
		if (write(1, buffer, nbytes) != (unsigned int) nbytes) {
			ssh_channel_close(ch);
			ssh_channel_free(ch);
			return SSH_ERROR;
		}
	}

	if (nbytes < 0) {
		ssh_channel_close(ch);
		ssh_channel_free(ch);
		return SSH_ERROR;
	}

	ssh_channel_send_eof(ch);
	ssh_channel_close(ch);
	ssh_channel_free(ch);
	return rc;
}

int32_t split_path(char * dir, int32_t dir_size, char * file, int32_t file_size, const char *path) {
	int32_t rc = 0;

	*dir = 0x00;
	*file = 0x00;

	char * pch;
	char * temp;
	char *s0;
	const char *s1;
	if ( (pch = strrchr(path, '/')) == NULL ) {
		strncpy(dir, ".", dir_size);
		strncpy(file, path, file_size);
	} else {
		for(s0=dir,s1=path; ( s1<pch && s0<(dir+dir_size) ); s0++,s1++) {
			*s0 = *s1;
		}
		*s0 = 0x00;
		strncpy(file, (pch+1), file_size);
	};
	
	return rc;
}

#ifdef __SCP_SUBSYSTEM__

int32_t	scp_session_write(ssh_session s, const char *remote_path, const char *local_path) {
	int32_t rc = SSH_OK;
	ssh_scp	scp;
	char remote_path_dir[1024];
	char remote_path_file[1024];

	split_path(remote_path_dir, sizeof(remote_path_dir), remote_path_file, sizeof(remote_path_file), remote_path);

	if ((scp=ssh_scp_new(s, SSH_SCP_WRITE|SSH_SCP_RECURSIVE, remote_path_dir)) == NULL) {
		fprintf(stderr, "ssh_scp_new() failed: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	if ((rc = ssh_scp_init(scp)) != SSH_OK) {
		fprintf(stderr, "ssh_scp_init() failed : %s\n", ssh_get_error(s));
		ssh_scp_free(scp);
		return SSH_ERROR;
	}

	int32_t fd0 = 0;
	struct stat stat_fd0;

	if ((fd0 = open(local_path, O_RDONLY)) < 0) {
		fprintf(stderr, "cannot open local file %s\n", local_path);
		ssh_scp_close(scp);
		ssh_scp_free(scp);
		return SSH_ERROR;
	}

	if ((fstat(fd0, &stat_fd0)) < 0 ) {
		fprintf(stderr,"cannot fstat local file %s\n", local_path);
		ssh_scp_close(scp);
		ssh_scp_free(scp);
		return SSH_ERROR;
	}

	//	fprintf(stdout, "File:%s st_mode:%o st_size:%llu\n", local_path, stat_fd0.st_mode, stat_fd0.st_size);

	if ((rc = ssh_scp_push_file64(scp, remote_path_file, stat_fd0.st_size, ( stat_fd0.st_mode & 0777 ) )) != SSH_OK) {
		fprintf(stderr, "Can't open remote file: %s\n", ssh_get_error(s));
		ssh_scp_close(scp);
		ssh_scp_free(scp);
		return rc;
	}

	uint64_t size;
	int32_t mode;
	char *filename, *buffer;
	int32_t buffer_size;
	size_t  bytesRead, bytesWritten;
	uint64_t    totalBytesRead, totalBytesWritten;
	int32_t i;

	buffer_size = 1;
	if ((buffer = malloc(buffer_size)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		ssh_scp_close(scp);
		ssh_scp_free(scp);
		return SSH_ERROR;
	}

	totalBytesRead = 0;
	while((bytesRead = read(fd0, buffer, buffer_size))>0) {
		if ((rc = ssh_scp_write(scp, buffer, bytesRead)) != SSH_OK) {
			fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(s));
			ssh_scp_close(scp);
			ssh_scp_free(scp);
			return rc;
		}
		totalBytesRead+=bytesRead;
	};

	if (bytesRead<0) {
		fprintf(stderr, "Error Read.");
	}

	//fprintf(stdout, "Total bytes sent: %llu\n", totalBytesRead);

	free(buffer); buffer=NULL;
	close(fd0);

	ssh_scp_close(scp);
	ssh_scp_free(scp);
	return rc;
}

int32_t scp_receive(ssh_session s, ssh_scp scp, int32_t fd)
{
	int32_t rc = SSH_OK;
	uint64_t size;
	int32_t mode;
	char *filename, *buffer;
	int32_t buffer_size;
	size_t  bytesRead, bytesWritten;
	uint64_t    totalBytesRead, totalBytesWritten;
	int32_t i;

	if ((rc = ssh_scp_pull_request(scp)) != SSH_SCP_REQUEST_NEWFILE){
		fprintf(stderr, "Error receiving information about file: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	size = ssh_scp_request_get_size64(scp);
	filename = strdup(ssh_scp_request_get_filename(scp));
	mode = ssh_scp_request_get_permissions(scp);
	fprintf(stderr, "Receiving file %s, size %ld, permissions 0%o\n", filename, size, mode);
	free(filename);

	buffer_size = 65536;
	if ((buffer = malloc(buffer_size)) == NULL){
		fprintf(stderr, "Memory allocation error\n");
		return SSH_ERROR;
	}

	if ((rc = ssh_scp_accept_request(scp)) != SSH_OK){
		fprintf(stderr, "Error scp sccept request: %s\n", ssh_get_error(s));
		free(buffer);
		return rc;
	};

	totalBytesRead = 0;
	totalBytesWritten = 0;
	while((bytesRead = ssh_scp_read(scp, buffer, buffer_size)) > 0) {

		i = 0;
		while ((bytesWritten = write(fd, buffer+i, bytesRead-i)) > 0) {
			i += bytesWritten;
			if (i>=bytesRead) {break;};
		};
		totalBytesWritten += i;
		totalBytesRead += bytesRead;
		if (totalBytesRead >= size) {
			break;
		}
	}

	if (bytesRead == SSH_ERROR){
		fprintf(stderr, "Error receiving file data: %s\n", ssh_get_error(s));
		free(buffer);
		return bytesRead;
	}
	fchmod(fd, mode);
	printf("Done\n");
	fprintf(stdout,"Number of bytes read    : %ld\n", totalBytesRead);
	fprintf(stdout,"Number of bytes written : %ld\n", totalBytesWritten);
	free(buffer);

	if ((rc = ssh_scp_pull_request(scp)) != SSH_SCP_REQUEST_EOF){
		fprintf(stderr, "Unexpected request: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	return SSH_OK;
}

int32_t	scp_session_read(ssh_session s, const char *remote_path, const char *local_path) {
	int32_t rc = SSH_OK;
	ssh_scp scp;

	if ((scp=ssh_scp_new(s, SSH_SCP_READ, remote_path)) == NULL) {
		fprintf(stderr, "Error allocating scp session: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	if ((rc = ssh_scp_init(scp)) != SSH_OK){
		fprintf(stderr, "Error initializing scp session: code %s.\n", ssh_get_error(scp));
		ssh_scp_free(scp);
		return rc;
	}   

	int32_t fd;

	unlink(local_path);
	fd = open( local_path, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );

	if ((rc=scp_receive(s, scp, fd)) != SSH_OK) {
		close(fd);
		ssh_scp_close(scp);
		ssh_scp_free(scp);
		return rc;
	}   
	close(fd);

	ssh_scp_close(scp);
	ssh_scp_free(scp);

	return rc;
}


int32_t	scp_write(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path) {
	int32_t		rc = 0;
	ssh_session s;
	int verbosity = SSH_LOG_PROTOCOL;
	int32_t		ssh_port = port;
	char		user[256];
	char		pass[256];

	if ((s = ssh_new()) == NULL)
	{
		fprintf(stderr, "ssh_new() failed.\n");
		return -1;
	}

	ssh_options_set(s, SSH_OPTIONS_HOST, host);
	//ssh_options_set(s, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(s, SSH_OPTIONS_PORT, &ssh_port);
	ssh_options_set(s, SSH_OPTIONS_USER, un);

	if ((rc = ssh_connect(s)) != SSH_OK) {
		fprintf(stderr, "ssh_connect(%s@%s:%d) failed: %s\n", un, host, ssh_port, ssh_get_error(s));
		ssh_free(s);
		return rc;
	}

	if ((rc = verify_knownhost(s)) != SSH_OK) {
		fprintf(stderr, "verify_knownhost(%s) failed: %s\n", host, ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	//	Test auth methods NONE
	rc = ssh_userauth_none(s, NULL);
	if ( rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR ) {
		fprintf(stderr, "ssh_userauth_none() failed: %s\n", ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	int32_t method;
	method = ssh_userauth_list(s, NULL);

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		rc = authenticate_kbdint(s, pw);
		if (rc != SSH_AUTH_SUCCESS) {
			fprintf(stderr, "authenticate_kbdint() failed.\n");
			ssh_disconnect(s);
			ssh_free(s);
			return -1;
		}
	} else {
		fprintf(stderr, "SSH_AUTH_METHOD_INTERACTIVE not supported.\n");
		ssh_disconnect(s);
		ssh_free(s);
		return -1;
	}

	//
	//	write
	//	fprintf(stderr, "scp_session_write(%s => %s)\n", local_path, remote_path);
	//
	rc = scp_session_write(s, remote_path, local_path);
	if (rc != SSH_OK) {
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}
	
	ssh_disconnect(s);
	ssh_free(s);

	return rc;
}

int32_t	scp_read(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path) {
	int32_t		rc = 0;
	ssh_session s;
	int verbosity = SSH_LOG_PROTOCOL;
	int32_t		ssh_port = port;
	char		user[256];
	char		pass[256];

	if ((s = ssh_new()) == NULL)
	{
		fprintf(stderr, "ssh_new() failed.\n");
		return -1;
	}

	ssh_options_set(s, SSH_OPTIONS_HOST, host);
	//ssh_options_set(s, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(s, SSH_OPTIONS_PORT, &ssh_port);
	ssh_options_set(s, SSH_OPTIONS_USER, un);

	if ((rc = ssh_connect(s)) != SSH_OK) {
		fprintf(stderr, "ssh_connect(%s@%s:%d) failed: %s\n", un, host, ssh_port, ssh_get_error(s));
		ssh_free(s);
		return rc;
	}

	if ((rc = verify_knownhost(s)) != SSH_OK) {
		fprintf(stderr, "verify_knownhost(%s) failed: %s\n", host, ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	//	Test auth methods NONE
	rc = ssh_userauth_none(s, NULL);
	if ( rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR ) {
		fprintf(stderr, "ssh_userauth_none() failed: %s\n", ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	int32_t method;
	method = ssh_userauth_list(s, NULL);

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		rc = authenticate_kbdint(s, pw);
		if (rc != SSH_AUTH_SUCCESS) {
			fprintf(stderr, "authenticate_kbdint() failed.\n");
			ssh_disconnect(s);
			ssh_free(s);
			return -1;
		}
	} else {
		fprintf(stderr, "SSH_AUTH_METHOD_INTERACTIVE not supported.\n");
		ssh_disconnect(s);
		ssh_free(s);
		return -1;
	}

	// Read
	rc = scp_session_read(s, remote_path, local_path);
	if (rc != SSH_OK) {
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	ssh_disconnect(s);
	ssh_free(s);

	return rc;
}

#endif


#ifdef __SFTP_SUBSYSTEM__

int32_t	sftp_session_write(ssh_session s, const char *remote_path, const char *local_path) {
	int32_t rc = SSH_OK;
	sftp_session sftp;
	sftp_file f;
	int access_type = O_WRONLY | O_CREAT | O_TRUNC;

	sftp = sftp_new(s);
	if (sftp == NULL) {
		fprintf(stderr, "Error allocating SFTP session: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	rc = sftp_init(sftp);
	if (rc != SSH_OK) {
		fprintf(stderr, "Error initializing SFTP session: code %d.\n", sftp_get_error(sftp));
		sftp_free(sftp);
		return rc;
	}

	f = sftp_open(sftp, remote_path, access_type, S_IRWXU);
	if (f == NULL) {
		fprintf(stderr, "Can't open file %s for writing: %s\n", remote_path, ssh_get_error(s));
		sftp_free(sftp);
		return SSH_ERROR;
	}

	int32_t fd0 = 0;
	struct stat stat_fd0;

	if ((fd0 = open(local_path, O_RDONLY)) < 0) {
		fprintf(stderr, "cannot open local file %s\n", local_path);
		sftp_close(f);
		sftp_free(sftp);
		return SSH_ERROR;
	}

	if ((fstat(fd0, &stat_fd0)) < 0 ) {
		fprintf(stderr,"cannot fstat local file %s\n", local_path);
		sftp_close(f);
		sftp_free(sftp);
		return SSH_ERROR;
	}

	//	fprintf(stdout, "File:%s st_mode:%o st_size:%llu\n", local_path, stat_fd0.st_mode, stat_fd0.st_size);

	uint64_t size;
	int32_t mode;
	char *filename, *buffer;
	int32_t buffer_size;
	size_t  bytesRead, bytesWritten;
	uint64_t    totalBytesRead, totalBytesWritten;
	int32_t i;

	buffer_size = 1;
	if ((buffer = malloc(buffer_size)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		sftp_close(f);
		sftp_free(sftp);
		return SSH_ERROR;
	}

	totalBytesRead = 0;
	totalBytesWritten = 0;
	while((bytesRead = read(fd0, buffer, buffer_size))>0) {
		bytesWritten = sftp_write(f, buffer, bytesRead);
		if (bytesWritten != bytesRead)	{
			fprintf(stderr, "Can't write data to file: %s\n", ssh_get_error(s));
			free(buffer); buffer=NULL;
			close(fd0);
			sftp_close(f);
			sftp_free(sftp);
			return SSH_ERROR;
		}
		
		totalBytesRead+=bytesRead;
		totalBytesWritten+=bytesWritten;
	};

	if (bytesRead<0) {
		fprintf(stderr, "Error reading file %s.", local_path);
		free(buffer); buffer=NULL;
		close(fd0);
		sftp_close(f);
		sftp_free(sftp);
		return SSH_ERROR;
	}

	fprintf(stderr, "Total bytes sent: %lu\n", totalBytesWritten);

	free(buffer); buffer=NULL;
	close(fd0);


	rc = sftp_close(f);
	if (rc != SSH_OK) {
		fprintf(stderr, "Can't close the written file: %s\n", ssh_get_error(s));
		sftp_free(sftp);
		return rc;
	}
  
	sftp_free(sftp);
	return rc;
}

int32_t	sftp_session_read(ssh_session s, const char *remote_path, const char *local_path) {
	int32_t rc = SSH_OK;
	sftp_session sftp;
	sftp_file f;
	int access_type = O_RDONLY;
	int32_t fd;

	unlink(local_path);
	fd = open( local_path, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if (fd < 0) {
		fprintf(stderr, "Cannot create local file %s\n", local_path);
		return SSH_ERROR;
	}

	sftp = sftp_new(s);
	if (sftp == NULL) {
		fprintf(stderr, "Error allocating SFTP session: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}

	rc = sftp_init(sftp);
	if (rc != SSH_OK) {
		fprintf(stderr, "Error initializing SFTP session: code %d.\n", sftp_get_error(sftp));
		sftp_free(sftp);
		return rc;
	}
 
	f = sftp_open(sftp, remote_path, access_type, 0);
	if (f == NULL) {
		fprintf(stderr, "Can't open file for reading: %s\n", ssh_get_error(s));
		return SSH_ERROR;
	}
 
	char *filename, *buffer;
	int32_t buffer_size;
	size_t  bytesRead, bytesWritten;
	uint64_t    totalBytesRead, totalBytesWritten;

	buffer_size = 65536;
	if ((buffer = malloc(buffer_size)) == NULL){
		fprintf(stderr, "Memory allocation error\n");
		return SSH_ERROR;
	}


	totalBytesRead = 0;
	totalBytesWritten = 0;
	for (;;) {
		bytesRead = sftp_read(f, buffer, sizeof(buffer));
		if (bytesRead == 0) {
			break; // EOF
		} else if (bytesRead < 0) {
			fprintf(stderr, "Error while reading file: %s\n", ssh_get_error(s));
			sftp_close(f);
			close(fd);
			sftp_free(sftp);
			return SSH_ERROR;
		}
		totalBytesRead+=bytesRead;
		bytesWritten = write(fd, buffer, bytesRead);
		if (bytesWritten != bytesRead) {
			fprintf(stderr, "Error writing: %s\n", strerror(errno));
			sftp_close(f);
			close(fd);
			sftp_free(sftp);
			return SSH_ERROR;
		}
		totalBytesWritten+=bytesWritten;
	}

	if (buffer) {free(buffer);buffer=NULL;}

	rc = sftp_close(f);
	if (rc != SSH_OK) {
		fprintf(stderr, "Can't close the read file: %s\n", ssh_get_error(s));
		close(fd);
		sftp_free(sftp);
		return rc;
	}

	close(fd);
	sftp_free(sftp);

	fprintf(stderr, "Total bytes received: %lu\n", totalBytesWritten);

	return rc;
}


int32_t	sftp_send(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path) {

	int32_t		rc = 0;
	ssh_session s;
	int verbosity = SSH_LOG_PROTOCOL;
	int32_t		ssh_port = port;
	char		user[256];
	char		pass[256];

	if ((s = ssh_new()) == NULL)
	{
		fprintf(stderr, "ssh_new() failed.\n");
		return -1;
	}

	ssh_options_set(s, SSH_OPTIONS_HOST, host);
	//ssh_options_set(s, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(s, SSH_OPTIONS_PORT, &ssh_port);
	ssh_options_set(s, SSH_OPTIONS_USER, un);

	if ((rc = ssh_connect(s)) != SSH_OK) {
		fprintf(stderr, "ssh_connect(%s@%s:%d) failed: %s\n", un, host, ssh_port, ssh_get_error(s));
		ssh_free(s);
		return rc;
	}

	if ((rc = verify_knownhost(s)) != SSH_OK) {
		fprintf(stderr, "verify_knownhost(%s) failed: %s\n", host, ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	//	Test auth methods NONE
	rc = ssh_userauth_none(s, NULL);
	if ( rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR ) {
		fprintf(stderr, "ssh_userauth_none() failed: %s\n", ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	int32_t method;
	method = ssh_userauth_list(s, NULL);

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		rc = authenticate_kbdint(s, pw);
		if (rc != SSH_AUTH_SUCCESS) {
			fprintf(stderr, "authenticate_kbdint() failed.\n");
			ssh_disconnect(s);
			ssh_free(s);
			return -1;
		}
	} else {
		fprintf(stderr, "SSH_AUTH_METHOD_INTERACTIVE not supported.\n");
		ssh_disconnect(s);
		ssh_free(s);
		return -1;
	}

	//	write
	rc = sftp_session_write(s, remote_path, local_path);
	if (rc != SSH_OK) {
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}
	
	ssh_disconnect(s);
	ssh_free(s);

	return rc;
}

int32_t	sftp_receive(const char *host, const int32_t port, const char *un, const char *pw, const char *remote_path, const char *local_path) {
	int32_t		rc = 0;
	ssh_session s;
	int verbosity = SSH_LOG_PROTOCOL;
	int32_t		ssh_port = port;
	char		user[256];
	char		pass[256];

	if ((s = ssh_new()) == NULL)
	{
		fprintf(stderr, "ssh_new() failed.\n");
		return -1;
	}

	ssh_options_set(s, SSH_OPTIONS_HOST, host);
	//ssh_options_set(s, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(s, SSH_OPTIONS_PORT, &ssh_port);
	ssh_options_set(s, SSH_OPTIONS_USER, un);

	if ((rc = ssh_connect(s)) != SSH_OK) {
		fprintf(stderr, "ssh_connect(%s@%s:%d) failed: %s\n", un, host, ssh_port, ssh_get_error(s));
		ssh_free(s);
		return rc;
	}

	if ((rc = verify_knownhost(s)) != SSH_OK) {
		fprintf(stderr, "verify_knownhost(%s) failed: %s\n", host, ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	//	Test auth methods NONE
	rc = ssh_userauth_none(s, NULL);
	if ( rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR ) {
		fprintf(stderr, "ssh_userauth_none() failed: %s\n", ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	int32_t method;
	method = ssh_userauth_list(s, NULL);

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		rc = authenticate_kbdint(s, pw);
		if (rc != SSH_AUTH_SUCCESS) {
			fprintf(stderr, "authenticate_kbdint() failed.\n");
			ssh_disconnect(s);
			ssh_free(s);
			return -1;
		}
	} else {
		fprintf(stderr, "SSH_AUTH_METHOD_INTERACTIVE not supported.\n");
		ssh_disconnect(s);
		ssh_free(s);
		return -1;
	}

	// Read
	rc = sftp_session_read(s, remote_path, local_path);
	if (rc != SSH_OK) {
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	ssh_disconnect(s);
	ssh_free(s);

	return rc;
}

#endif



int32_t	remote_execute(const char *host, const int32_t port, const char *un, const char *pw, const char *cmdstr) {
	int32_t		rc = 0;
	ssh_session s;
	int verbosity = SSH_LOG_PROTOCOL;
	int32_t		ssh_port = port;
	char		user[256];
	char		pass[256];

	if ((s = ssh_new()) == NULL)
	{
		fprintf(stderr, "ssh_new() failed.\n");
		return -1;
	}

	ssh_options_set(s, SSH_OPTIONS_HOST, host);
	//ssh_options_set(s, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(s, SSH_OPTIONS_PORT, &ssh_port);
	ssh_options_set(s, SSH_OPTIONS_USER, un);

	if ((rc = ssh_connect(s)) != SSH_OK) {
		fprintf(stderr, "ssh_connect(%s@%s:%d) failed: %s\n", un, host, ssh_port, ssh_get_error(s));
		ssh_free(s);
		return rc;
	}

	if ((rc = verify_knownhost(s)) != SSH_OK) {
		fprintf(stderr, "verify_knownhost(%s) failed: %s\n", host, ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	//	Test auth methods NONE
	rc = ssh_userauth_none(s, NULL);
	if ( rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR ) {
		fprintf(stderr, "ssh_userauth_none() failed: %s\n", ssh_get_error(s));
		ssh_disconnect(s);
		ssh_free(s);
		return rc;
	}

	int32_t method;
	method = ssh_userauth_list(s, NULL);

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		rc = authenticate_kbdint(s, pw);
		if (rc != SSH_AUTH_SUCCESS) {
			fprintf(stderr, "authenticate_kbdint() failed.\n");
			ssh_disconnect(s);
			ssh_free(s);
			return -1;
		}
	} else {
		fprintf(stderr, "SSH_AUTH_METHOD_INTERACTIVE not supported.\n");
		ssh_disconnect(s);
		ssh_free(s);
		return -1;
	}

	// remote execution
	if ((rc = remote_session_execute(s, cmdstr)) != SSH_OK ) {
		fprintf(stderr, "remote_session_execute() failed.\n");
	}

	ssh_disconnect(s);
	ssh_free(s);

	return rc;
}

int32_t	read_linux_primary_password(char *pwd, size_t pwd_size) {
	int32_t		rc = 0;
	char *un = NULL;

	char prikey_path[1024];
	char lnxkey_path[1024];

	un = getlogin();
	snprintf(prikey_path, sizeof(prikey_path), "/home/%s/.ssh/id_rsa", un);
	snprintf(lnxkey_path, sizeof(lnxkey_path), "/home/%s/.ssh/id_rsa.linux.b64", un);

	char *v;
	if (( v = getenv("KWSSH_PRIKEY_PATH") ) != NULL )
		strncpy(prikey_path, v, sizeof(prikey_path));

	if (( v = getenv("KWSSH_LNXKEY_PATH") ) != NULL )
		strncpy(lnxkey_path, v, sizeof(lnxkey_path));

	#ifdef __KWSSH_DEBUG__
	WARN("prikey_path:%s", prikey_path);
	WARN("lnxkey_path:%s", lnxkey_path);
	#endif

	rc = read_linux_password(pwd, pwd_size, prikey_path, lnxkey_path);

	return rc;
};

static inline int32_t is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

int32_t	base64_decode(unsigned char *out, const char *in) {
	
	char *base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

	int in_len = strlen(in);
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];

	//unsigned char outstr[8192];
	unsigned char *ret = out;
	int32_t	outlen;

	outlen = 0;
	while (in_len-- && ( in[in_] != '=') && is_base64(in[in_])) {
		char_array_4[i++] = in[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++) {
				//	char_array_4[i] = base64_chars.find(char_array_4[i]);
				char *p = strchr(base64_chars, char_array_4[i]);
				if (p!=NULL) {
					char_array_4[i] = (p - base64_chars);
				} else {
					return -1;
				}
			}

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++) {
				//	ret += char_array_3[i];
				*ret = char_array_3[i];
				ret++;
				outlen++;
			}
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++) {
			//	char_array_4[j] = base64_chars.find(char_array_4[j]);
				char *p = strchr(base64_chars, char_array_4[j]);
				if (p!=NULL) {
					char_array_4[j] = (p - base64_chars);
				} else {
					return -1;
				}
		}

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) {
			//	ret += char_array_3[j];
				*ret = char_array_3[j];
				ret++;
				outlen++;
		}
	}

	return outlen;
}

int32_t	read_linux_password(char *pwd, size_t pwd_size, const char *prikey_path, const char *lnxkey_path) {

	int32_t		rc = 0;
	FILE		*fp;
	char		comstr[2048];
  	char		buffer[1035];

	//
	//	use openssl command to decript unix password using private key id_rsa
	//
	//		cat /home/wai/.ssh/id_rsa.linux.b64 | base64 -d | openssl pkeyutl -decrypt -inkey /home/wai/.ssh/id_rsa
	//
	//	Extract public key from RSA private key in PEM format file: id_rsa.pub.pem seems not be able to import using openssl
	//
	//		ssh-keygen -f /home/wai/.ssh/id_rsa.pub -e -m pem > /home/wai/.ssh/id_rsa.pub.pem
	//
	//	Extract public key from RSA private key in PEM format file: id_rsa.pub.pem
	//
	//		openssl rsa -in /home/wai/.ssh/id_rsa -pubout -out /home/wai/.ssh/id_rsa.pub.pem
	//
	//	openssl dgst -sha256 -sign <private-key> -out /tmp/sign.sha256 <file>
	//
	//	Encrypt password to : id_rsa.linux.b64 
	//
	//		echo "password" | openssl pkeyutl -encrypt -pubin -inkey /home/wai/.ssh/id_rsa.pub.pem -in - | base64 > /home/wai/.ssh/id_rsa.linux.b64
	//


	//
	//	To Do:
	//
	//		Use native openssl api call instead
	//

	//	echo "password" | openssl pkeyutl -encrypt -inkey /home/wai/.ssh/id_rsa -in - | base64 > /home/wai/.ssh/id_rsa.linux.b64.password
	//	cat /home/wai/.ssh/id_rsa.linux.b64.password | base64 -d | openssl pkeyutl -decrypt -inkey /home/wai/.ssh/id_rsa
	//
	//	const RSA *EVP_PKEY_get0_RSA(const EVP_PKEY *pkey);
	//	int RSA_private_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
	//

	// 1. Read file : lnxkey_path

	int fd0 = 0;
	char filebuf[8192];
	int32_t	bytesRead, bytesDecoded;
	int64_t totalBytesRead;

	if ((fd0 = open(lnxkey_path, O_RDONLY)) < 0) {
		WARN("cannot open local file %s", lnxkey_path);
		return -1;
	}

	totalBytesRead = 0;
	while((bytesRead = read(fd0, filebuf, sizeof(filebuf)))>0) {
		totalBytesRead+=bytesRead;
	};

	char *s0, *s1;
	for(s0=filebuf,s1=filebuf; *s1 != 0x00; s1++) {
		if ( (*s1 != 0x0D ) && (*s1 != 0x0A ) ) {
			*s0 = *s1; s0++;
		}
	}
	*s0 = 0x00;
	totalBytesRead = strlen(filebuf);

	// use equal sign to determine how many padding	zero to remove after base64 decode
	int32_t howmany_equal = 0;
	for(char *p=(filebuf+totalBytesRead-1); *p == '='; p--) {
		howmany_equal++;
	}

	totalBytesRead = totalBytesRead ;

	unsigned char lnxkey[4096];
	#ifdef __KWSSH_DEBUG__
	WARN("totalBytesRead=%ld", totalBytesRead);
	#endif

	bytesDecoded = -1;
	//	https://docs.openssl.org/3.1/man3/EVP_EncodeInit
	bytesDecoded = EVP_DecodeBlock((unsigned char*)lnxkey, (unsigned char*)filebuf, totalBytesRead);
	if ( bytesDecoded < 0 ) {
		WARN("EVP_DecodeBlock() returns: %d", bytesDecoded);
	}
	bytesDecoded -= howmany_equal;
	
	// lnxkey, bytesDecoded
	#ifdef __KWSSH_DEBUG__
	for( int i=0; i< bytesDecoded; i++) {
		fprintf(stderr, " %02x", lnxkey[i]);
		if ( i % 16 == 15 ) fprintf(stderr, " \n");
	}
	fprintf(stderr, " \n");
	WARN("EVP_DecodeBlock() returns: %d", bytesDecoded);
	#endif

	close(fd0);

	//	https://docs.openssl.org/3.1/man3/BIO_s_file/
	//	https://docs.openssl.org/1.0.2/man3/BIO_f_base64/
	//	https://docs.openssl.org/3.1/man3/PEM_read_bio_PrivateKey/
	//	https://docs.openssl.org/3.1/man3/EVP_PKEY_decrypt/

	OSSL_LIB_CTX *libctx;
	
	libctx = OSSL_LIB_CTX_new();

	BIO *bp;
	int32_t rc_oss = 0;
	//	int BIO_read_filename(BIO *b, char *name);
	bp = BIO_new(BIO_s_file());
	rc_oss = BIO_read_filename(bp, "/home/wai/.ssh/id_rsa_backup");
	if (rc_oss == 0) {
		WARN0("BIO_read_filename() failed");
		OSSL_LIB_CTX_free(libctx);
		return -1;
	}

	EVP_PKEY_CTX *pkey_ctx;
	EVP_PKEY *prikey;

	prikey = PEM_read_bio_PrivateKey(bp, &prikey, NULL, "private key");
	if (prikey == NULL) {
		WARN0("PEM_read_bio_PrivateKey() failed.");
		BIO_free(bp);
		OSSL_LIB_CTX_free(libctx);
		return -1;
	} else {
		#ifdef __KWSSH_PRINT_RSA_PRIVATE_KEY__
		BIO *bio_out;
		bio_out = BIO_new(BIO_s_file());

		if (bio_out == NULL)
			WARN0("BIO_new(BIO_s_file()) failed.")
		else {
			if (BIO_set_fp(bio_out, stdout, BIO_NOCLOSE) <= 0)
				WARN("BIO_set_fp(bio_out, stdout, BIO_NOCLOSE) failed.");
		}
		EVP_PKEY_print_private(bio_out, prikey, 0, NULL);
		BIO_free(bio_out);
		#endif
	}
	BIO_free(bp);

	#ifdef __KWSSH_USE_RSA__
	RSA *rsa = EVP_PKEY_get0_RSA(prikey);
	if (rsa == NULL) {
		WARN0("EVP_PKEY_get0_RSA(prikey) failed.")
	}
	#endif
	
	int32_t rc0 = 0;

	pkey_ctx = EVP_PKEY_CTX_new_from_pkey(libctx, prikey, NULL);

	if (pkey_ctx == NULL) {
		WARN0("EVP_PKEY_CTX_new_from_pkey() failed.")
	} else {

		if ((rc0 = EVP_PKEY_pairwise_check(pkey_ctx)) <= 0) {
			WARN("EVP_PKEY_pairwise_check() failed (%d).", rc0);
		}

		if ((rc0 = EVP_PKEY_decrypt_init(pkey_ctx)) <= 0) {
			WARN("EVP_PKEY_decrypt_init() failed (%d).", rc0);
		}

		//	if ((rc0=EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_OAEP_PADDING)) <= 0) {
		if ((rc0=EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING)) <= 0) {
			WARN("EVP_PKEY_CTX_set_rsa_padding() failed (%d).", rc0);
		}

		size_t	outlen = 0;
		//	Determine buffer length
		//	lnxkey, bytesDecoded
		if ((rc0=EVP_PKEY_decrypt(pkey_ctx, NULL, &outlen, (const unsigned char *)lnxkey, bytesDecoded)) <= 0) {
			WARN("EVP_PKEY_decrypt() failed (%d).", rc0);
		} else {
			#ifdef __KWSSH_DEBUG__
			WARN("EVP_PKEY_decrypt() determine outlen:%zu", outlen);
			#endif
		}
		
		unsigned char *out;
		out = OPENSSL_malloc(outlen);

		if (!out) {
			WARN0("OPENSSL_malloc() failed.");
		}

		if ((rc0 = EVP_PKEY_decrypt(pkey_ctx, out, &outlen, (const unsigned char *)lnxkey, bytesDecoded)) <= 0) {
			WARN("EVP_PKEY_decrypt() failed (%d).", rc0);
		} else {
			#ifdef __KWSSH_DEBUG__
			WARN("EVP_PKEY_decrypt() succeed (%d).", rc0);
			for(int i=0; i< outlen; i++) {
				fprintf(stderr, "%c", out[i]);
			}
			fprintf(stderr, "\n");
			#endif
			//	pwd, size_t pwd_size
			pwd_size = outlen;
			memcpy(pwd, out, outlen);
			out[outlen] = 0x00;
		}

		#ifdef __KWSSH_USE_RSA__
		//	int RSA_private_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
		rc0 = RSA_private_decrypt(bytesDecoded, (const unsigned char *)lnxkey, out, rsa, RSA_PKCS1_PADDING);
		if (rc0 < 0) {
			WARN("RSA_private_decrypt() failed (%d).", rc0);
		} else {
			#ifdef __KWSSH_DEBUG__
			WARN("RSA_private_decrypt() succeed (%d).", rc0);
			#endif
			//	pwd, size_t pwd_size
			pwd_size = outlen;
			memcpy(pwd, out, outlen);
			out[outlen] = 0x00;
		}
		#endif

		OPENSSL_free(out);
	}

	EVP_PKEY_CTX_free(pkey_ctx);
	OSSL_LIB_CTX_free(libctx);

	//	##################################################################################
	//	/home/wai/.ssh/id_rsa.linux.b64 | base64 -d | openssl pkeyutl -decrypt -inkey /home/wai/.ssh/id_rsa
	//	cat /home/wai/.ssh/id_rsa.linux.b64 | base64 -d | openssl pkeyutl -decrypt -inkey /home/wai/.ssh/id_rsa_backup
	#ifdef __KWSSH_USE_OPENSSL_BINARY__
	snprintf(comstr, sizeof(comstr), "cat %s | base64 -d | openssl pkeyutl -decrypt -inkey %s", lnxkey_path, prikey_path);

	//	Open the command for reading.
	if ((fp = popen(comstr, "r")) == NULL) {
		fprintf(stderr, "Failed to run command: %s\n", comstr);
		return -1;
	}

	//	Read first line from the output
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		strncpy(pwd, buffer, pwd_size);
		break;
	}
	pclose(fp);

	// strip()
	for(char *ch=(pwd+strlen(pwd)); ch>pwd; ch--) {
		if ( *ch == 0x0D || *ch == 0x0A ) { *ch = 0x00; }
	}

	if (*pwd == 0x00) {
		rc = -1;
	}
	#endif

	// strip()
	for(char *ch=(pwd+strlen(pwd)); ch>pwd; ch--) {
		if ( *ch == 0x0D || *ch == 0x0A ) { *ch = 0x00; }
	}

	if (*pwd == 0x00) {
		rc = -1;
	}

	return rc;
}


