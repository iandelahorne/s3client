s3client
========

C Implementation of an Amazon S3 client library. 

Currently it's in "experimental" stage, will be refactored into a library later.

Developed by Ian Delahorne <ian.delahorne@gmail.com> and released under MIT license.


Dependencies:
* OpenSSL
* libxml2 
* libcurl
 
Currently it compiles on my Mac, Linux support  will be added later.
Amazon secret ID and key are defined in s3_secret.h

Todo
----
In no particular order, features that are left are:

- Bucket creation
- Bucket deletion
- Object deletion
- Support for key prefixes / paths
- Content-Types, MD5 sum of uploaded content
- Contents of bucket lists should be a BSD queue.h list
- Make into an actual library
- ACLs
- Clean up and generalize CURL / signing code.
