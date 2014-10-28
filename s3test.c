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

#include "s3.h"
#include "s3xml.h"

#ifdef LINUX
#include <bsd/string.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <curl/curl.h>


struct s3_content {
	char *key;
	char *lastmod; /* time_t */
	size_t size;
	char *etag;
	TAILQ_ENTRY(s3_content) list;
};

TAILQ_HEAD(s3_content_head, s3_content);

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
				printf("node xmlNodeGetContent: %s\n",value);
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

void 
walk_xpath_nodes(xmlNodeSetPtr nodes, void *data) {
	struct s3_content_head *head;
	xmlNodePtr cur;
	int size;
	int i;

	head = (struct s3_content_head *) data;

	size = (nodes) ? nodes->nodeNr : 0;
	printf("size is %d nodes\n", size);
	for (i = 0; i < size ; i++) {
		if (nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
			cur = nodes->nodeTab[i];
			
			struct s3_content *content = parse_content(cur->children);
			TAILQ_INSERT_TAIL(head, content, list);
		}
	}
}



void 
walk_xpath_prefixes(xmlNodeSetPtr nodes, void *data) {
	xmlNodePtr cur;
	int size;
	int i;

	size = (nodes) ? nodes->nodeNr : 0;
	printf("prefix size is %d nodes\n", size);
	for (i = 0; i < size ; i++) {
		if (nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
			cur = nodes->nodeTab[i];
			parse_prefixes(cur->children);
		}
	}
}

void 
libxml_do_stuff(char *str) {
	struct s3_content_head *contents;
	struct s3_content *c;
	xmlDocPtr doc;

	contents = malloc(sizeof (*contents));
	TAILQ_INIT(contents);

	doc = xmlReadMemory(str, strlen(str), "noname.xml", NULL, 0);

	/* 
	 * Since Amazon uses an XML Namespace, we need to declare it
	 * and use it as a prefix in Xpath queries, even though it's
	 * not written out in the tag names - libxml2 follows the
	 * standard where others don't
	 */
	s3_execute_xpath_expr(doc, (const xmlChar *)"//amzn:Contents", walk_xpath_nodes, contents);
	s3_execute_xpath_expr(doc, (const xmlChar *)"//amzn:CommonPrefixes", walk_xpath_prefixes, NULL);

	TAILQ_FOREACH(c, contents, list) {
		printf("\tKey %s\n", c->key);
	}
	
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
      	asprintf(&url, "http://%s.%s/?delimiter=/%s%s", bucket, s3->base_url, prefix ? "&prefix=" : "", prefix ? prefix : "");

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
	
	s3 = s3_init(S3_KEY, S3_SECRET, "s3.amazonaws.com");
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
