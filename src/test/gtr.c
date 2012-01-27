#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
	struct rusage ru;

	if (nice(-20) < 0)
		perror("gtr: nice");

	if (fork() == 0)
		execvp(argv[1], &argv[1]);
	wait3(NULL, 0, &ru);

	fprintf(stderr, "user time: %g\tsystem time: %g\n",
		(double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec
			* 0.000001,
		(double)ru.ru_stime.tv_sec + (double)ru.ru_stime.tv_usec
			* 0.000001);
	return 0;
}
