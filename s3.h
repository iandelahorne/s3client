/*
 * Copyright (c) 2013, Ian Delahorne <ian.delahorne@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.  
 */

#ifndef _S3_H
#define _S3_H

#include <string.h>

struct s3_string {
	char *ptr;
	size_t len;
	size_t uploaded;
};


struct S3 {
	char *secret;
	char *id;
	char *base_url;
	char *proxy;
};



struct s3_string * s3_string_init();
size_t s3_string_curl_writefunc(void *ptr, size_t len, size_t nmemb, struct s3_string *s);
size_t s3_string_curl_readfunc(void *ptr, size_t len, size_t nmemb, struct s3_string *s);
void s3_string_free(struct s3_string *str);

char * s3_hmac_sign(const char *key, const char *str, size_t len);
char * s3_md5_sum(const char *content, size_t len);

char * s3_make_date();

void s3_perform_op(struct S3 *s3, const char *method, const char *url, const char *sign_data, const char *date, struct s3_string *out, struct s3_string *in, const char *content_md5, const char *content_type);

void s3_get(struct S3 *s3, const char *bucket, const char *key, struct s3_string *out);
void s3_delete(struct S3 *s3, const char *bucket, const char *key);
void s3_put(struct S3 *s3, const char *bucket, const char *key, const char *content_type, const char *data, size_t len);

#endif /* _S3_H */
