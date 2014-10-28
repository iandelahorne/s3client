#include "s3content.h"
#ifdef LINUX
#include <bsd/string.h>
#endif
#include <string.h>

void 
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
s3_contents_free(struct s3_content_head *contents) {
	struct s3_content *c;

	while ((c = TAILQ_FIRST(contents)) != NULL) {
		TAILQ_REMOVE(contents, c, list);
		s3_content_free(c);
	}
	free(contents);
}


struct s3_content *
s3_parse_content(xmlNode *root) {
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
#ifdef DEBUG
				printf("node type: Element, name: %s\n", node->name);
				printf("node xmlNodeGetContent: %s\n",value);
#endif
			}
			xmlFree(value);
		}
	}
	return content;	
}
