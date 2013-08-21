s3client
========

C Implementation of an Amazon S3 client library. 

Currently it's in "experimental" stage, will be refactored into a library later.

Dependencies:
* OpenSSL
* libxml2 
* libcurl
 
Currently it compiles on OSX and Linux.
Amazon secret ID and key are defined in s3_secret.h

Todo
----
In no particular order, features that are left are:

- Bucket creation
- Bucket deletion
- Support for key prefixes / paths
- Contents of bucket lists should be a BSD queue.h list
- Make into an actual library
- ACLs
- Clean up and generalize CURL / signing code (partially done)


Author
======

- Ian Delahorne (<ian.delahorne@gmail.com>)

License
=======

Copyright (c) 2013, Ian Delahorne <ian.delahorne@gmail.com>

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.  
