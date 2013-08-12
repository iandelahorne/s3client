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


#include <stdio.h>
#include <string.h>

#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include <curl/curl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


#include "s3.h"
#include "s3_secret.h"

struct s3_content {
	char *key;
	char *lastmod; /* time_t */
	size_t size;
	char *etag;
};

struct s3_xml_data {
	int level;
};
#define S3_SECRET_LENGTH 128
#define S3_ID_LENGTH 128

struct S3 {
	char *secret;
	char *id;
	char *base_url;
};

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

static unsigned char *
md5_sum(const char *content, size_t len) {

	const EVP_MD *md = EVP_md5();
	unsigned char *md_value;
	EVP_MD_CTX *ctx;
	unsigned int md_len;
	
	md_value = malloc(EVP_MAX_MD_SIZE);
	
	ctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(ctx, md, NULL);
	EVP_DigestUpdate(ctx, content, len);
	EVP_DigestFinal_ex(ctx, md_value, &md_len);
	
	EVP_MD_CTX_destroy(ctx);

	return md_value;
}

static struct S3 *
s3_init(const char *id, const char *secret, const char *base_url) {
	struct S3 *s3 = malloc(sizeof (struct S3));

	s3->id = malloc(S3_ID_LENGTH);
	s3->secret = malloc(S3_SECRET_LENGTH);
	/* XXX better length */
	s3->base_url = malloc(256); 
	
	strlcpy(s3->id, id, S3_ID_LENGTH);
	strlcpy(s3->secret, secret, S3_SECRET_LENGTH);
	strlcpy(s3->base_url, base_url, 255);

	return s3;
}

static void
s3_free(struct S3 *s3) {
	free(s3->id);
	free(s3->secret);
	free(s3->base_url);
	
	free(s3);
}

void
register_namespaces(xmlXPathContextPtr ctx, const xmlChar *nsList) {
	xmlChar* nsListDup;
	xmlChar* prefix;
	xmlChar* href;
	xmlChar* next;
	
	nsListDup = xmlStrdup(nsList);
	if(nsListDup == NULL) {
		fprintf(stderr, "Error: unable to strdup namespaces list\n");
		return;
	}
	
	next = nsListDup; 
	while(next != NULL) {
		/* skip spaces */
		while((*next) == ' ') next++;
		if((*next) == '\0') break;
		
		/* find prefix */
		prefix = next;
		next = (xmlChar*)xmlStrchr(next, '=');
		if(next == NULL) {
			fprintf(stderr,"Error: invalid namespaces list format\n");
			xmlFree(nsListDup);
			return;
		}
		*(next++) = '\0';	
		
		/* find href */
		href = next;
		next = (xmlChar*)xmlStrchr(next, ' ');
		if(next != NULL) {
			*(next++) = '\0';	
		}
		
		/* do register namespace */
		if(xmlXPathRegisterNs(ctx, prefix, href) != 0) {
			fprintf(stderr,"Error: unable to register NS with prefix=\"%s\" and href=\"%s\"\n", prefix, href);
			xmlFree(nsListDup);
			return;
		}
	}
	
	xmlFree(nsListDup);
}

struct s3_content *
parse_content(xmlNode *root) {
	struct s3_content *content = malloc(sizeof(struct s3_content));
	xmlNode *node = NULL;
	for (node = root; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			xmlChar *value =  xmlNodeGetContent(node->children);
			size_t len = strlen((const char *) value);

			if (strcasecmp("key", (const char *)node->name) == 0) {
				content->key = malloc(len + 2);
				strlcpy(content->key, (const char *)value, len + 1);
			} else if (strcasecmp("lastmodified", (const char *)node->name) == 0) {
				content->lastmod = malloc(len + 2);
				strlcpy(content->lastmod, (const char *)value, len + 1);
			} else if (strcasecmp("etag", (const char *)node->name) == 0) {
				content->etag = malloc(len + 2);
				strlcpy(content->etag, (const char *)value, len + 1);
			} else {
				
				printf("node type: Element, name: %s\n", node->name);
				printf("node xmlsNodeGetContent: %s\n",value);
			}
			xmlFree(value);
		}
	}
	return content;
	
}
/* 
void
libxml_walk_nodes(xmlNode *root) {
	xmlNode *node = NULL;
	for (node = root; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			printf("node type: Element, name: %s\n", node->name);
		}
		libxml_walk_nodes(node->children);
	}
}
*/

void 
walk_xpath_nodes(xmlNodeSetPtr nodes) {
	xmlNodePtr cur;
	int size;
	int i;

	size = (nodes) ? nodes->nodeNr : 0;
	printf("size is %d nodes\n", size);
	for (i = 0; i < size ; i++) {
		if (nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
			cur = nodes->nodeTab[i];
			
			/* Push this onto a list of contents, return list */
			struct s3_content *content = parse_content(cur->children);
			printf("Content parsed for key %s\n", content->key);
		}
	}
}
/* Should change this to a callback */
void 
execute_xpath_expr(const xmlDocPtr doc, const xmlChar *xpath_expr, const xmlChar *ns_list) {
	xmlXPathContextPtr xpath_ctx;
	xmlXPathObjectPtr xpath_obj;

	xpath_ctx = xmlXPathNewContext(doc);
	if (xpath_ctx == NULL) {
		fprintf(stderr,"Error: unable to create new XPath context\n");
	}

	register_namespaces(xpath_ctx, ns_list);
	
	xpath_obj = xmlXPathEvalExpression(xpath_expr, xpath_ctx);
	if(xpath_obj == NULL) {
		fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n", xpath_expr);
	}
	walk_xpath_nodes(xpath_obj->nodesetval);

	xmlXPathFreeObject(xpath_obj);
	xmlXPathFreeContext(xpath_ctx); 
}

void 
libxml_do_stuff(char *str) {
	xmlDocPtr doc;
	doc = xmlReadMemory(str, strlen(str), "noname.xml", NULL, 0);
	/* xmlNode *root_element = xmlDocGetRootElement(doc); */

	/* Since Amazon uses an XML Namespace, we need to declare it and use it as a prefix in Xpath queries, even though it's  */
	execute_xpath_expr(doc, (const xmlChar *)"//amzn:Contents", (const xmlChar *)"amzn=http://s3.amazonaws.com/doc/2006-03-01/");
	
	xmlFreeDoc(doc);
}



char *
s3_make_date() {
	char *date;
	time_t now;
	struct tm tm;

	date = malloc(128);
	now = time(0);
	tm = *gmtime(&now);
	strftime(date, 128, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return date;

}
/* Add return values later */
void
s3_perform_op(struct S3 *s3, const char *url, const char *sign_data, const char *date, struct s3_string *str) {
	char *digest;
	char *hdr;
	
	CURL *curl;
	struct curl_slist *headers = NULL;



	fprintf(stderr, "data to sign:%s\n", sign_data);

	digest = hmac_sign(s3->secret, sign_data, strlen(sign_data));
	/* fprintf(stderr, "Authentication: AWS %s:%s\n", s3->id, digest); */
	
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

	/* http://stackoverflow.com/questions/2329571/c-libcurl-get-output-into-a-string */

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, s3_string_curl_writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, str);

	curl_easy_setopt(curl, CURLOPT_URL, url);
#if 0
	curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost:8080");
#endif
	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	free(digest);	
}

void
s3_perform_upload(struct S3 *s3, const char *url, const char *sign_data, const char *date, struct s3_string *in, struct s3_string *out) {
	char *digest;
	char *hdr;
	
	CURL *curl;
	struct curl_slist *headers = NULL;

	digest = hmac_sign(s3->secret, sign_data, strlen(sign_data));
	/* fprintf(stderr, "Authentication: AWS %s:%s\n", s3->id, digest); */
	
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

	/* http://stackoverflow.com/questions/2329571/c-libcurl-get-output-into-a-string */

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, s3_string_curl_writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, s3_string_curl_readfunc);
	curl_easy_setopt(curl, CURLOPT_READDATA, in);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, in->len);

	curl_easy_setopt(curl, CURLOPT_URL, url);
#if 0
	curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost:8080");
#endif

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);


	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	free(digest);	
}

static void
s3_list_bucket(struct S3 *s3, const char *bucket) {
	char *date;
	char *sign_data;	
	char *url;
	struct s3_string *str;
	const char *method = "GET";


	str = s3_string_init();		
	date = s3_make_date();

	asprintf(&sign_data, "%s\n\n\n%s\n/%s/", method, date, bucket);	
      	asprintf(&url, "http://%s.%s/?delimiter=/", bucket, s3->base_url);

	s3_perform_op(s3, url, sign_data, date, str);

	libxml_do_stuff(str->ptr);	
	printf("%s\n", str->ptr);

	s3_string_free(str);
	free(sign_data);
	free(date);
}


static void
s3_get(struct S3 *s3, const char *bucket, const char *key) {
	const char *method = "GET";
	char *sign_data;
	char *date;
	char *url;
	struct s3_string *str;

	date = s3_make_date();

	asprintf(&sign_data, "%s\n\n\n%s\n/%s/%s", method, date, bucket, key);	
	asprintf(&url, "http://%s.%s/%s", bucket, s3->base_url, key);
	str = s3_string_init();		
	
	s3_perform_op(s3, url, sign_data, date, str);
	printf("%ld\n", str->len);
	
	s3_string_free(str);

	free(sign_data);
	free(date);
	free(url);
}

static void
s3_put(struct S3 *s3, const char *bucket, const char *key, const char *content_type, const char *data, size_t len) {
	const char *method = "PUT";
	char *sign_data;
	char *date;
	char *url;
	unsigned char *md5;
	char content_md5[33];
	struct s3_string *in, *out;

	in = s3_string_init();
	out = s3_string_init();

	in->ptr = realloc(in->ptr, len + 1);
	memcpy(in->ptr, data, len);
	in->len = len;

	md5 = md5_sum(data, len);
	for(int i = 0; i < 16; ++i)
		sprintf(&content_md5[i*2], "%02x", (unsigned int)md5[i]);
	
	/* printf("md5 is %s\n", content_md5); */
	date = s3_make_date();
	/* asprintf(&sign_data, "%s\n%s\n%s\n%s\n/%s/%s", method, content_md5, content_type, date, bucket, key); */
	asprintf(&sign_data, "%s\n%s\n%s\n%s\n/%s/%s", method, "", "", date, bucket, key); 

	asprintf(&url, "http://%s.%s/%s", bucket, s3->base_url, key);

	s3_perform_upload(s3, url, sign_data, date, in, out);
	printf("url %s\n", url);
	printf("data to sign %s\n", sign_data);
	printf("\n%s\n", out->ptr);

	s3_string_free(in);
	s3_string_free(out);

	free(url);
	free(md5);
	free(sign_data);
}


int main (int argc, char **argv) {
	struct S3 *s3; 
	
	s3 = s3_init(S3_KEY, S3_SECRET, "s3.amazonaws.com");
	const char *bucket = S3_BUCKET;
	
	s3_list_bucket(s3, bucket); 
	
	s3_get(s3, bucket, "Towel-Dog.jpg");
	char *val = "foo bar gazonk";
	s3_put(s3, bucket, "trattmule.txt", "text/plain", val, strlen(val));
	s3_free(s3);

}
