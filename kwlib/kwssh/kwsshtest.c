#include <stdio.h>
#include <stdlib.h>

#include "kwssh.h"


//	cd /home/wai/misc/workspace/kwlib/kwssh/build && rm -rf ./* && cmake .. && make && export KWSSH_LNXKEY_PATH=/home/wai/.ssh/id_rsa.linux.b64.password && ./kwsshtest 
int32_t main(int argc, char **argv) {
	int32_t rc = 0;

	char *host = "localhost";
	int32_t port = 22;
	char *un = NULL;
	char pw[256];
	char f[1024];

	un = getlogin();

	//	ssh-keygen -f /home/wai/.ssh/id_rsa.pub -e -m pem > /home/wai/.ssh/id_rsa.pub.pem
	//	Extract public key from RSA private key in PEM format
	//	openssl rsa -in /home/wai/.ssh/id_rsa -pubout -out /home/wai/.ssh/id_rsa.pub.pem
	//
	//	openssl dgst -sha256 -sign <private-key> -out /tmp/sign.sha256 <file>
	//	echo "password" | openssl pkeyutl -encrypt -pubin -inkey /home/wai/.ssh/id_rsa.pub.pem -in - | base64 > /home/wai/.ssh/id_rsa.linux.b64
	//	cat /home/wai/.ssh/id_rsa.linux.b64 | base64 -d | openssl pkeyutl -decrypt -inkey /home/wai/.ssh/id_rsa

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

	WARN0("Test scp_write(): /home/wai/.ssh/id_rsa.pub.pem => /tmp/uploaded.txt");
	rc = scp_write(host, port, un, pw, "/tmp/uploaded.txt","/home/wai/.ssh/id_rsa.pub.pem");
	if (rc != 0) {
		WARN("scp_write() failed with return code:%d",rc);
		exit(-1);
	}

	WARN0("Test scp_read(): /home/wai/.ssh/id_rsa.pub.pem => /tmp/downloaded.txt");
	rc = scp_read(host, port, un, pw, "/home/wai/.ssh/id_rsa.pub.pem","/tmp/downloaded.txt");
	if (rc != 0) {
		WARN("scp_read() failed with return code:%d",rc);
		exit(-1);
	}

#endif

#ifdef __SFTP_SUBSYSTEM__

	WARN0("Test sftp_send(): /home/wai/.ssh/id_rsa.pub.pem => /tmp/uploaded.txt");
	rc = sftp_send(host, port, un, pw, "/tmp/uploaded.txt","/home/wai/.ssh/id_rsa.pub.pem");
	if (rc != 0) {
		WARN("sftp_send() failed with return code:%d",rc);
		exit(-1);
	}

	WARN0("Test sftp_receive(): /home/wai/.ssh/id_rsa.pub.pem => /tmp/downloaded.txt");
	rc = sftp_receive(host, port, un, pw, "/home/wai/.ssh/id_rsa.pub.pem","/tmp/downloaded.txt");
	if (rc != 0) {
		WARN("sftp_receive() failed with return code:%d",rc);
		exit(-1);
	}

#endif


	WARN0("RUN: ls -ld /home/wai/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt");
	rc = system("ls -ld /home/wai/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt");

	WARN0("RUN: md5sum /home/wai/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt");
	rc = system("md5sum /home/wai/.ssh/id_rsa.pub.pem /tmp/downloaded.txt /tmp/uploaded.txt");

	WARN0("Completed\n");
}

