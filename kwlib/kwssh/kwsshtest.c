#include <stdio.h>
#include <stdlib.h>

#include "kwssh.h"


//	cd ~/misc/workspace/kwlib/kwssh/build && rm -rf ./* && cmake .. && make && export KWSSH_LNXKEY_PATH=~/.ssh/id_rsa.linux.b64 && ./kwsshtest 
int32_t main(int argc, char **argv) {
	int32_t rc = 0;

	char *host = "localhost";
	int32_t port = 22;
	char *un = NULL;
	char pw[256];
	char f[1024];

	un = getlogin();
	char prikey_path[1024];
	char lnxkey_path[1024];
	char remote_path[1024];
	char local_path[1024];

	snprintf(prikey_path, sizeof(prikey_path), "/home/%s/.ssh/id_rsa", un);
	snprintf(lnxkey_path, sizeof(lnxkey_path), "/home/%s/.ssh/id_rsa.linux.b64", un);

	//	ssh-keygen -f ~/.ssh/id_rsa.pub -e -m pem > ~/.ssh/id_rsa.pub.pem
	//	Extract public key from RSA private key in PEM format
	//	openssl rsa -in ~/.ssh/id_rsa -pubout -out ~/.ssh/id_rsa.pub.pem
	//
	//	openssl dgst -sha256 -sign <private-key> -out /tmp/sign.sha256 <file>
	//	echo "password" | openssl pkeyutl -encrypt -pubin -inkey ~/.ssh/id_rsa.pub.pem -in - | base64 > ~/.ssh/id_rsa.linux.b64
	//	cat ~/.ssh/id_rsa.linux.b64 | base64 -d | openssl pkeyutl -decrypt -inkey ~/.ssh/id_rsa

	//	ssh -o StrictHostKeyChecking=no -o BatchMode=yes -i ~/.ssh/id_rsa user@host "command"

	rc = read_linux_primary_password(pw, sizeof(pw));
	if (rc != 0) {
		WARN("read_linux_primary_password() failed with return code:%d",rc);
		exit(-1);
	}

	WARN0("Test remote_execute(): whoami;hostname");
	rc = remote_execute(host, port, un, pw, "whoami;hostname");
	if (rc != 0) {
		WARN("remote_execute() failed with return code:%d",rc);
		exit(-1);
	}

#ifdef __SCP_SUBSYSTEM__
	snprintf(remote_path, sizeof(remote_path), "/tmp/uploaded.txt");
	snprintf(local_path, sizeof(local_path), "/home/%s/.ssh/id_rsa.pub.pem", un);
	WARN("Test scp_write(): %s => %s", local_path, remote_path);
	rc = scp_write(host, port, un, pw, remote_path, local_path);
	if (rc != 0) {
		WARN("scp_write() failed with return code:%d",rc);
		exit(-1);
	}

	snprintf(remote_path, sizeof(remote_path), "/home/%s/.ssh/id_rsa.pub.pem", un);
	snprintf(local_path, sizeof(local_path), "/tmp/downloaded.txt");
	WARN("Test scp_read(): %s => %s", remote_path, local_path);
	rc = scp_read(host, port, un, pw, remote_path, local_path);
	if (rc != 0) {
		WARN("scp_read() failed with return code:%d",rc);
		exit(-1);
	}

#endif

#ifdef __SFTP_SUBSYSTEM__

	snprintf(remote_path, sizeof(remote_path), "/tmp/uploaded.txt");
	snprintf(local_path, sizeof(local_path), "/home/%s/.ssh/id_rsa.pub.pem", un);
	WARN("Test sftp_send(): %s => %s", local_path, remote_path);
	rc = sftp_send(host, port, un, pw, remote_path, local_path);
	if (rc != 0) {
		WARN("sftp_send() failed with return code:%d",rc);
		exit(-1);
	}

	snprintf(remote_path, sizeof(remote_path), "/home/%s/.ssh/id_rsa.pub.pem", un);
	snprintf(local_path, sizeof(local_path), "/tmp/downloaded.txt");

	WARN("Test sftp_receive(): %s => %s", remote_path, local_path);
	rc = sftp_receive(host, port, un, pw, remote_path, local_path);
	if (rc != 0) {
		WARN("sftp_receive() failed with return code:%d",rc);
		exit(-1);
	}

#endif

	char command_str[1024];
	snprintf(command_str, sizeof(command_str), "ls -ld /home/%s/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt", un);

	WARN("RUN: %s", command_str);
	rc = system(command_str);

	snprintf(command_str, sizeof(command_str), "md5sum /home/%s/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt", un);
	WARN("RUN: %s", command_str);
	rc = system(command_str);

	WARN0("Completed\n");
}

