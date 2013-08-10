#ifndef _S3_H
#define _S3_H

struct s3_string {
	char *ptr;
	size_t len;
};

struct s3_string * s3_string_init();
size_t s3_string_curl_writefunc(void *ptr, size_t len, size_t nmemb, struct s3_string *s);
void s3_string_free(struct s3_string *str);

#endif /* _S3_H */
