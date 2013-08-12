#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "s3.h"

size_t
s3_string_curl_writefunc(void *ptr, size_t len, size_t nmemb, struct s3_string *s) {
	size_t new_len = s->len + len  *nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr+s->len, ptr, len*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return len*nmemb;
}

size_t
s3_string_curl_readfunc(void *ptr, size_t len, size_t nmemb, struct s3_string *s) {
	size_t left = s->len - s->uploaded;
	size_t max_chunk = len * nmemb;
	size_t retcode = left < max_chunk ? left : max_chunk;
	
	memcpy(ptr, s->ptr + s->uploaded, retcode); 
	
	s->uploaded += retcode;  // <-- save progress
	return retcode;
}

struct s3_string *
s3_string_init() {
	struct s3_string *s;
	s = malloc(sizeof (struct s3_string));
	s->len = 0;
	s->uploaded = 0;
	s->ptr = malloc(s->len+1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
	return s;
}

void
s3_string_free(struct s3_string *str) {
	free(str->ptr);
	free(str);
}
