#include <iostream>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	FILE *matches;
	matches = fopen("matches.txt", "a");
	char to_append[1000];

	sprintf(to_append, "Championship ID: 1\n\tPlayer razvi won against player costica.\n\tScores have been adjusted accordingly, with +200 for razvi and -150 for costica.\n");
	fprintf(matches, "%s", to_append);

	return 0;
}
