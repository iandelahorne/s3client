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

#ifdef LINUX
#define _GNU_SOURCE
#include <bsd/string.h>
#endif

#include <stdio.h>
#include <string.h>

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

#define S3_SECRET_LENGTH 128
#define S3_ID_LENGTH 128

static struct S3 *
s3_init(const char *id, const char *secret, const char *base_url, const char *proxy) {
	struct S3 *s3 = malloc(sizeof (struct S3));

	s3->id = malloc(S3_ID_LENGTH);
	s3->secret = malloc(S3_SECRET_LENGTH);
	/* XXX better length */
	s3->base_url = malloc(256); 
	
	strlcpy(s3->id, id, S3_ID_LENGTH);
	strlcpy(s3->secret, secret, S3_SECRET_LENGTH);
	strlcpy(s3->base_url, base_url, 255);
	
	s3->proxy = NULL;

	return s3;
}

static void
s3_free(struct S3 *s3) {
	free(s3->id);
	free(s3->secret);
	free(s3->base_url);
	
	free(s3);
}

static void 
s3_content_free(struct s3_content *content) {
	if (content->key)
		free(content->key);
	if (content->lastmod)
		free(content->lastmod);
	if (content->etag)
		free(content->etag);

	free(content);
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
void
parse_prefixes(xmlNode *root) {
	xmlNode *node = NULL;
	for (node = root; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			xmlChar *value =  xmlNodeGetContent(node->children);
			/* size_t len = strlen((const char *) value); */
			
			printf("Prefix node type: Element, name: %s\n", node->name);
			printf("Prefix node xmlsNodeGetContent: %s\n",value);
			xmlFree(value);
		}
	}
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
walk_xpath_nodes(xmlNodeSetPtr nodes, void *data) {
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
			printf("\tKey %s\n", content->key);
			s3_content_free(content);
		}
	}
}

void 
execute_xpath_expr(const xmlDocPtr doc, const xmlChar *xpath_expr, const xmlChar *ns_list, void (*nodeset_cb)(xmlNodeSetPtr, void *), void *cb_data) {
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

	nodeset_cb(xpath_obj->nodesetval, cb_data);

	xmlXPathFreeObject(xpath_obj);
	xmlXPathFreeContext(xpath_ctx); 
}

void 
walk_xpath_prefixes(xmlNodeSetPtr nodes, void *data) {
	xmlNodePtr cur;
	int size;
	int i;

	size = (nodes) ? nodes->nodeNr : 0;
	printf("size is %d nodes\n", size);
	for (i = 0; i < size ; i++) {
		if (nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
			cur = nodes->nodeTab[i];
			parse_prefixes(cur->children);
		}
	}
}

void 
libxml_do_stuff(char *str) {
	xmlDocPtr doc;
	doc = xmlReadMemory(str, strlen(str), "noname.xml", NULL, 0);
	/* xmlNode *root_element = xmlDocGetRootElement(doc); */

	/* Since Amazon uses an XML Namespace, we need to declare it and use it as a prefix in Xpath queries, even though it's  */
	execute_xpath_expr(doc, (const xmlChar *)"//amzn:Contents", (const xmlChar *)"amzn=http://s3.amazonaws.com/doc/2006-03-01/",
			   walk_xpath_nodes, NULL);
	execute_xpath_expr(doc, (const xmlChar *)"//amzn:CommonPrefixes", (const xmlChar *)"amzn=http://s3.amazonaws.com/doc/2006-03-01/",
			   walk_xpath_prefixes, NULL);
	
	xmlFreeDoc(doc);
	xmlCleanupParser();
}





static void
s3_list_bucket(struct S3 *s3, const char *bucket, const char *prefix) {
	char *date;
	char *sign_data;	
	char *url;
	struct s3_string *str;
	const char *method = "GET";


	str = s3_string_init();		
	date = s3_make_date();

	asprintf(&sign_data, "%s\n\n\n%s\n/%s/", method, date, bucket);	
      	asprintf(&url, "http://%s.%s/?delimiter=/%s%s", bucket, s3->base_url, prefix ? "&prefix=" : "", prefix);

	s3_perform_op(s3, method, url, sign_data, date, str, NULL, NULL, NULL);

	libxml_do_stuff(str->ptr);	
	printf("%s\n", str->ptr);

	s3_string_free(str);
	free(url);
	free(sign_data);
	free(date);
}



int main (int argc, char **argv) {
	struct S3 *s3; 
	struct s3_string *out;
	
	s3 = s3_init(S3_KEY, S3_SECRET, "s3.amazonaws.com", NULL);
	const char *bucket = S3_BUCKET;
	
	s3_list_bucket(s3, bucket, NULL);
	s3_list_bucket(s3, bucket, "foo/bar/");
	
	out = s3_string_init();
	s3_get(s3, bucket, "Towel-Dog.jpg", out);
	s3_string_free(out);
       
	char *val = "foo bar gazonk";
	s3_put(s3, bucket, "trattmule.txt", "text/plain", val, strlen(val));
	
	s3_delete(s3, bucket, "trattmule.txt");
	s3_free(s3);
	
	return 0;
}
