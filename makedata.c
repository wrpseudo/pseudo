#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int
main(void) {
	int i, j, k, l;
	/* templateish form used so it's obvious that it's long enough */
	char name[] = "dir_%d/dir_%d/%d%d.txt";
	for (i = 0; i < 10; ++i) {
		snprintf(name, sizeof(name), "dir_%d", i);
		mkdir(name, 0755);
		for (j = 0; j < 40; ++j) {
			snprintf(name, sizeof(name), "dir_%d/dir_%d", i, j);
			mkdir(name, 0755);
			for (k = 0; k < 10; ++k) {
				for (l = 0; l < 10; ++l) {
					FILE *fp;
					snprintf(name, sizeof(name),
						"dir_%d/dir_%d/%d%d.txt",
						i, j, k, l);
					fp = fopen(name, "w");
					if (fp) {
						fprintf(fp, "dummy file.\n");
						fclose(fp);
					}
				}
			}
		}
	}
	return 0;
}
