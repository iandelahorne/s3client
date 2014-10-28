#ifndef _S3_CONTENT_H
#define _S3_CONTENT_H
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <sys/queue.h>

struct s3_content {
	char *key;
	char *lastmod; /* time_t */
	size_t size;
	char *etag;
	TAILQ_ENTRY(s3_content) list;
};

TAILQ_HEAD(s3_content_head, s3_content);

void s3_content_free(struct s3_content *content);
void s3_contents_free(struct s3_content_head *contents);

struct s3_content * s3_parse_content(xmlNode *root);
#endif /* _S3_CONTENT_H */
