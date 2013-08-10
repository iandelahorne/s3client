#include <stdio.h>
#include <string.h>

#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include <curl/curl.h>

#include <expat.h>
#define NS_DELIM '.'

#include "s3.h"
/*
 * Example code that signs a request for listing the bucket's contents
 * Should trans even need to touch S3? Probably does.
 * Can we upload directly to S3 from PHP? From the form?
 * Why shouldn't we upload directly to S3 from PHP or form?
 * 
 */

struct s3_contents {
	char *key;
	char *lastmod; /* time_t */
	size_t size;
	char *etag;
};

struct s3_xml_data {
	int level;
};

static void XMLCALL
s3_start_tag(void *user_data, const XML_Char *name, const XML_Char **attrs) {
	char *tagns = NULL;
	struct s3_xml_data *data = user_data;
	const char *nsdelim = strrchr(name, NS_DELIM);
	int i;	
	char kbuf[1024];

	if (nsdelim) {
		tagns = strndup(name, nsdelim - name);
		name = nsdelim + 1;
	}
	for (i = 0; i < data->level; i++) 
		fprintf(stderr, "\t");	
	data->level += 1;
	
	fprintf(stderr, "<%s>\n", name);
	if (strcmp(name, "Contents") == 0) {
		fprintf(stderr, "Starting new Contents\n");
	}
/*
	for (; *attrs; attrs += 2) {
		char *v;
		int vsz;
		
		nsdelim = strchr(*attrs, NS_DELIM);
		if (nsdelim || !tagns)
			snprintf(kbuf, sizeof (kbuf), "attr.%s", *attrs);
		else
			snprintf(kbuf, sizeof (kbuf), "attr.%s.%s", tagns, *attrs);
		
		vsz = strlen(*(attrs + 1)) * 2 + 1;
		v = malloc(vsz);
		strlcpy(v, *(attrs + 1), vsz);
		for (i = 0; i < data->level; i++) 
			fprintf(stderr, "\t");	
		fprintf(stderr, "attr %s:%s\n",kbuf, v);
	}
*/

}

void XMLCALL
s3_end_tag(void *user_data, const XML_Char *name) {
	struct s3_xml_data *data = user_data;
	char *tagns = NULL;
        const char *nsdelim = strrchr(name, NS_DELIM);
	int i;
        if (nsdelim) {
                tagns = strndup(name, nsdelim - name);
                name = nsdelim + 1;
        }
	data->level -= 1;
	for (i = 0; i < data->level; i++) 
		fprintf(stderr, "\t");	
        fprintf(stderr, "</%s>\n", name);
}
void XMLCALL
s3_data(void *user_data, const XML_Char *s, int len) {
	int i;
	struct s3_xml_data *data = user_data;
	for (i = 0; i < data->level; i++) 
		fprintf(stderr, "\t");	
	fprintf(stderr, "%.*s\n", len, s);
}
static void xml_do_stuff(char *str) {
	XML_Parser parser;
	struct s3_xml_data *data = malloc(sizeof (struct s3_xml_data));
	data->level = 0;
	parser = XML_ParserCreateNS("UTF-8", NS_DELIM);
	
	XML_SetElementHandler(parser, s3_start_tag, s3_end_tag);
	XML_SetCharacterDataHandler(parser, s3_data);
	XML_SetUserData(parser, data);

	if (!XML_Parse(parser, str, strlen(str), 1)) {
		fprintf(stderr, "Parser error!\n");	/* error */	
		fprintf(stderr, "str is %s\n", str);
	}	

	XML_ParserFree(parser);
	free(data);
}

char *
hmac_sign(const char *key, const char *str, size_t len) {
	unsigned char *digest; 
	char *buf;
	unsigned int digest_len = EVP_MAX_MD_SIZE; /* HMAC_Final needs at most EVP_MAX_MD_SIZE bytes */
	HMAC_CTX ctx;
	BIO *bmem, *b64;
	BUF_MEM *bufptr;
	
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

	/* Setup HMAC context, init with sha1 and our key*/ 
	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, key, strlen((char *)key), EVP_sha1(), NULL);

	/* Create Base64 BIO filter that outputs to memory */
	b64  = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64  = BIO_push(b64, bmem);

	/* Give us a buffer to write result into */
	digest = malloc(digest_len);

	/* Push data into HMAC */
	HMAC_Update(&ctx, (unsigned char *)str, (unsigned int)len);

	/* Flush HMAC data into buffer */
	HMAC_Final(&ctx, digest, &digest_len);

	/* Write data into BIO, flush and fetch the data */
	BIO_write(b64, digest, digest_len);
	(void) BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bufptr);
	
	buf = malloc(bufptr->length);
	memcpy(buf, bufptr->data, bufptr->length - 1);
	buf[bufptr->length - 1] = '\0';

	BIO_free_all(b64);
	HMAC_CTX_cleanup(&ctx);

	free(digest);
	return buf;
}

#define S3_SECRET_LENGTH 128
#define S3_ID_LENGTH 128

struct S3 {
	char *secret;
	char *id;
};


static struct S3 *
s3_init(const char *id, const char *secret) {
	struct S3 *s3 = malloc(sizeof (struct S3));

	s3->id = malloc(S3_ID_LENGTH);
	s3->secret = malloc(S3_SECRET_LENGTH);
	
	strlcpy(s3->id, id, S3_ID_LENGTH);
	strlcpy(s3->secret, secret, S3_SECRET_LENGTH);

	return s3;
}

static void
s3_free(struct S3 *s3) {
	free(s3->id);
	free(s3->secret);
	
	free(s3);
}

/* http://stackoverflow.com/questions/2329571/c-libcurl-get-output-into-a-string */

int main (int argc, char **argv) {
	struct S3 *s3; 
	struct s3_string *str;
	
#ifdef AMZN
	s3 = s3_init("REDACTED", "REDACTED");
	const char *bucket = "drutmule";
	
#else
	s3 = s3_init("O5GQYKFKYGPFLB7E0MI4", "10c7hqOBkH2AqcmzNmTjFSz-fgNZHndKrzj9Kg==");
	const char *bucket = "test-bucket";
#endif	
	const char *method = "GET";

	char *date;
	char *data, *digest;
	char *hdr;

	time_t now;
	struct tm tm;

	CURL *curl;
	struct curl_slist *headers = NULL;

	str = s3_string_init();		
	date = malloc(128);
	now = time(0);
	tm = *gmtime(&now);
	strftime(date, 128, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	
	data = malloc(1024);
	snprintf(data, 1023, "%s\n\n\n%s\n/%s/", method, date, bucket);
	
	fprintf(stderr, "data is %s\n", data);

	digest = hmac_sign(s3->secret, data, strlen(data));
	
	fprintf(stderr, "Authentication: AWS %s:%s\n", s3->id, digest);

	curl = curl_easy_init();
	
	hdr = malloc(1024);

	snprintf(hdr, 1023, "Date: %s", date);
	headers = curl_slist_append(headers, hdr);

	snprintf(hdr, 1023, "Authorization: AWS %s:%s", s3->id, digest);
	headers = curl_slist_append(headers, hdr);

	free(hdr);
	
	
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
/*	curl_easy_setopt(curl, CURLOPT_HEADER, 1); */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, s3_string_curl_writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, str);

#ifdef AMZN
	curl_easy_setopt(curl, CURLOPT_URL, "http://drutmule.s3.amazonaws.com/?delimiter=/");
#else	
	curl_easy_setopt(curl, CURLOPT_URL, "http://test-bucket.s3.amazonaws.com/?delimiter=/");
	curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost:8080");
#endif
	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	xml_do_stuff(str->ptr);	
	printf("%s\n", str->ptr);
	s3_string_free(str);
	s3_free(s3);
	free(data);
	free(date);
	free(digest);
	return 0;
}
