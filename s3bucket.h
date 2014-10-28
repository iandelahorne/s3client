/*
 * Copyright (c) 2013,2014 Ian Delahorne <ian.delahorne@gmail.com>
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

#ifndef _S3_BUCKET_H
#define _S3_BUCKET_H
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <sys/queue.h>
#include "s3.h"
#include "s3xml.h"

struct s3_bucket_entry {
	char *key;
	char *lastmod; /* time_t */
	size_t size;
	char *etag;
	TAILQ_ENTRY(s3_bucket_entry) list;
};

TAILQ_HEAD(s3_bucket_entry_head, s3_bucket_entry);

void s3_bucket_entry_free(struct s3_bucket_entry *entry);
void s3_bucket_entries_free(struct s3_bucket_entry_head *entries);

struct s3_bucket_entry_head * s3_list_bucket(struct S3 *s3, const char *bucket, const char *prefix);
#endif /* _S3_BUCKET_ENTRY_H */
