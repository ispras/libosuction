#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

int main()
{
	int sockfd, peerfd;
	struct sockaddr_un sa = {.sun_family = AF_UNIX};
	memcpy(sa.sun_path, "\0ldprivd", 8);

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0
	    || bind(sockfd, (void *) &sa, sizeof sa) < 0
	    || listen(sockfd, 128) < 0)
		goto err;

	int fileno = 0;
#if 0
#pragma omp parallel
#pragma omp master
	while ((peerfd = accept(sockfd, 0, 0)) >= 0)
#pragma omp task firstprivate(peerfd) shared(fileno)
#else
        while ((peerfd = accept(sockfd, 0, 0)) >= 0)
#endif
	{
		int cmdlen;
		char *cmdline = 0;
		FILE *f;
		if (!(f = fdopen(peerfd, "r"))
		    || fscanf(f, "%d:", &cmdlen) != 1
		    || !(cmdline = malloc(cmdlen))
		    || fread(cmdline, cmdlen, 1, f) != 1)
			fprintf(stderr, "read error\n");
		else
			printf("%s\n", cmdline);
		free(cmdline);
		int fn = __sync_fetch_and_add(&fileno, 1);
		char namebuf[32];
		snprintf(namebuf, sizeof namebuf, "deps-%03d", fn);
		FILE *out = fopen(namebuf, "w");
		char buf[4096];
		size_t len;
		while ((len = fread(buf, 1, sizeof buf, f)) > 0)
			fwrite(buf, 1, len, out);
		fclose(out);
		fclose(f);
	}
	return 0;
err:
	fprintf(stderr, "%s\n", strerror(errno));
	return 1;
}
