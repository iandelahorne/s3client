#include <stdio.h>
#include <string.h>

#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include <curl/curl.h>

#include <expat.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define NS_DELIM '.'

#include "s3.h"
/*
 * Example code that signs a request for listing the bucket's contents
 * Should trans even need to touch S3? Probably does.
 * Can we upload directly to S3 from PHP? From the form?
 * Why shouldn't we upload directly to S3 from PHP or form?
 * 
 */

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

# if 0

static void XMLCALL
s3_start_tag(void *user_data, const XML_Char *name, const XML_Char **attrs) {
	char *tagns = NULL;
	struct s3_xml_data *data = user_data;
	const char *nsdelim = strrchr(name, NS_DELIM);
	int i;	
	/* char kbuf[1024]; */

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
#endif 
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
s3_perform_op(struct S3 *s3, const char *method, const char *url, const char *sign_data, const char *date, struct s3_string *str) {
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

	s3_perform_op(s3, method, url, sign_data, date, str);

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
	
	s3_perform_op(s3, method, url, sign_data, date, str);
	printf("%ld\n", str->len);
	
	s3_string_free(str);

	free(sign_data);
	free(date);
	free(url);
}


int main (int argc, char **argv) {
	struct S3 *s3; 
	
	s3 = s3_init("REDACTED", "REDACTED", "s3.amazonaws.com");
	const char *bucket = "REDACTED";
	
	s3_list_bucket(s3, bucket);
	
	s3_get(s3, bucket, "Towel-Dog.jpg");

	return 0;
	s3_free(s3);

}
