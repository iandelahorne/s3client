#ifndef _S3_INTERNAL_H
#define _S3_INTERNAL_H

char * s3_make_date(void);
void s3_perform_op(struct S3 *s3, const char *method, const char *url, const char *sign_data, const char *date, struct s3_string *out, struct s3_string *in, const char *content_md5, const char *content_type);

#endif //_S3_INTERNAL_H
