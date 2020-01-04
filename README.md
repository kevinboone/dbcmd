<h1>dbcmd</h1>

Version 0.0.2, March 2017

<h2>What is this?</h2>

<code>dbcmd</code> is a very simple command-line utility for Linux,
for uploading files to, and downloading them from, the Dropbox server.
It can list files on the server and retrieve their meta-data,
delete files, and rename them. In its purposes, it is a little like
<code>rsync</code>.
<p/>
<code>dbcmd</code> uses the Dropbox published HTTP API for all
its server operations. It is written entirely in C, and has no
dependencies except <code>libcurl</code>, and standard Linux commands.
It was written specifically for embedded systems that cannot run the
Dropbox proprietary client, and do not have the dependencies needed
to use any of the proprietary API libraries. It will run on Linux desktop
systems too, but there is less need for it in such an environment.
In particular, I wrote it to use on ARM-based Chromebooks, which can't
run the proprietary client, and don't have storage to synchronize an
entire Dropbox account even if they could. 
<p/>
In fact, <code>dbcmd</code> is not an appropriate tool for
synchronizing a complete Dropbox account (unless there are only a 
few files). Synchronization using the HTTP API is very slow and, if
there are many files, will use a great deal of memory. 
<p/>
Rather, <code>dbcmd</code> is intended to be used to copy files
from the server, work on them, and then copy changes back to the server. 

<h2>Usage examples</h2>

<pre class="codeblock">
# List the contents of a folder on the Dropbox server 
dbcmd list /docs/accounts/

# Download files from the /docs/accounts/ folder to a local directory
dbcmd --recursive get /docs/accounts/ accounts/

# Upload changed files from the local directory to the server
dbcmd put --recursive accounts/ /docs/accounts/

# Rename a file
dbcmd rename /docs/accounts/may207.doc /docs/accounts/may2017.doc

# Delete a Dropbox folder without prompting for confirmation 
dbcmd --yes /docs/accounts/temp/
</pre>


<h2>Prerequisites</h2>

<code>dbcmd</code> is designed to run on modern Linux systems. 
It uses <code>libcurl</code>, but the <code>curl</code> utility
need not be installed. 
To read the manual pages you will need the <code>man</code> utility and
its dependencies (particularly <code>groff</code>). These utilities are likely
to be present in all Linux distributions except, perhaps, those for
embedded systems.

<h2>Building</h2>

The usual:

<pre class="codeblock">
$ make
$ sudo make install
</pre>

<h2>Getting help</h2>

<code>man dbcmd</code> is a good place to start. Individual commands have their
own manual pages, which can be shown, for example, like this:

<pre class="codeblock">
$ dbcmd help get 
</pre>

<h2>Authentication</h2>

Like all applications that use Dropbox's public API, <code>dbcmd</code> 
authentication uses the OAUTH2 
protocol, which provides the application with a token that authorizes
it on the server. The first time you run <code>dbcmd</code> you will see
this message: 

<pre class="codeblock">
Please enter this URL into your browser, and allow dbcmd
to get access to your Dropbox files: 

https://www.dropbox.com/oauth2/authorize?response_type=code&amp;client_id=yorayt4h2bxk8x3

Then enter the authorization code here: 
</pre>

Following the link in a browser will take you to the Dropbox authorization page,
where you can select to authorize this utility. Authorization will provide a string
of numbers and letters which you should enter (or cut and paste) 
at the prompt. This string is not the authorization token itself, but is converted
into one by a further API operation. 
The token is then stored in a file <code>$HOME/.dbcmd_token</code>. Some care needs to
be taken with this file because the token can be used to impersonate you
on the Dropbox server -- not easily, but it can be done by a person with sufficient
knowledge of the Dropbox protocols.
<p/>
The token can be revoked at any time using the Dropbox website. If this happens,
and it is not intentional, then you can use the <code>--auth</code> switch
with any <code>dbcmd</code> command to restart the authorization process.

<h2>General notes</h2>

<h3>Dropbox folder naming</h3>

Dropbox folder names <i>must</i> start a forward-slash, and components are
separated by further foward-slashes, as in Unix. In principle, 
the Dropbox API uses an empty string to signify the top-level folder (which
is actually quite logical) but
this utility expects an ordinary Unix forward-slash, because
entering an empty string on the command line is a bit awkward.
The <code>get</code> and
<code>put</code> commands attach special significance to the presence
or absence of a trailing forward-slash on directories names,
as <code>rsync</code> does. In some cases, using a trailing forward
slash to indicate that you know that the name refers to a folder can 
speed things up a little by removing the need for another call on the
Dropbox server; in some cases, however, a trailing slash <i>can't</i>
be used on a folder name. Please see the manual pages for the individual
commands for more details on this issue, which has a number of
subtleties that need to be understood. In all cases, the
<code>--dry-run</code> option can be used to check what changes will
actually be made before making them. 

<h3>Recursive transfers</h3>

The <code>get</code> and <code>put</code> commands have a
<code>--recursive</code> switch, that indicates to upload or download from
subfolders. It is important to realize that, unlike <code>rsync</code> or the
Linux <code>cp</code> utility, recursive transfers can be used with a specific
fimename of file pattern.  For example

<pre class="codeblock">
dbcmd put --recursive $HOME/images/*.jpg /images/
</pre>

will copy all files in <code>$HOME/images</code> that match the pattern
<code>*.jpg</code>, creating folders on the server as necessary to maintain the
same directory structure.  
<p/>
It is also important to bear in mind that recursive transfers of large numbers
of files will take a long time, and use a great deal of memory, because
complete file lists have to be contructed, along with hash values for each
file, and passed over the wire.


<h3>Dropbox timestamps</h3>

Dropbox synchronization does not work by comparing timestamps -- it compares
file hashes. You can, in principle, upload a file to the server that has the
same hash as an existing file, but <i>the timestamp of the file on the server
is not changed</i> if you do. Attempting to use timestamps for synchronization
is thus rather problematic. 
<p/>
Dropbox stores two timestamps for each file -- a "server timestamp" and a
"client timestamp". The server timestamp is the time at which the file
was uploaded to the server. The client timetamp is not managed by Dropbox,
and can be set to anything. It could, in principle, be set to the timestamp
of the file on the local filesystem, and used for synchronization.
However, other Dropbox clients do not use the client timestamp -- not, at least,
in any consistent way; so it cannot be relied upon (and is not explicitly set by
the proprietary client).
<p/>
So, in practice, using <code>dbcmd</code> to synchronize files between clients
sharing the same Dropbox account will result in them being considered
"in sync" when all files have the same local timestamp as the server
timestamp. This is the same behaviour the proprietary Dropbox client
implements. 
<p/>
Calculating hashes takes much, much longer than checking a timestamp, but is a
more reliable way to tell whether two files are different or not.  However,
this extra time makes synchronization a longer operation than it would be if we
could just check timestamps.


<h2>Bugs and limitations</h2>

There is no limit -- other than time and memory -- on the number of files 
that can be listed, searched, or copied in one 
operation. However, since most operations first 
require building a list of files held on the server and the local
filesystem, and their hash values, and because Dropbox delivers its file
lists relatively slowly, it is impractical to work on huge sets of
files.
<p/>
There is additionally no limit on the size of a file that can be uploaded
or downloaded. However, Dropbox is not really designed for handling
huge files, and these operations are slow, and can use a great deal
of memory. For the record, uploads are performed in 4B blocks by default. 
Increasing this (e.g., <code>--buffersize=100</code>) 
can improve speed a little, but at the expense of
increased memory usage.
<p/>
The file containing the stored authorization token is not encrypted. It could
be encrypted, but this would require asking the user for a decryption key or
password on every operation, which would be a nuisance.
<p/>
Dropbox does not store Linux file attributes -- not even the owner's
permissions.  Files retrieved by <code>dbcmd</code> will usually have 644
permissions, regardless of what they had when the file was uploaded. 
<p/>
The Dropbox API is not always particularly detailed in its error 
responses. Something
that <code>dbcmd</code> reports as a permissions problem could relate to
a revoked authorization token, or just to an incorrect pathname on the server.
<p/>
Dropbox has a notion of file versions, and can retain deleted or modified
versions of the same file. <code>dbcmd</code> does not provide access to
previous versions or deleted files.
<p/>
Dropbox supports the use of multiple collaborators with access to the same
file. Dropbox won't stand in the way of such operations, but does not
(indeed can not) report who the owner of a file is, or indicate what 
permissions the user has on that file. It can't be used to change sharing behaviour.
<p/>
In principle, file transfer operations that specify multiple source
files require that the target be a directory. We don't really want
a situation where multiple files are copied onto the same file, 
overwriting it each time. <code>dbcmd</code> prevents this if
the situation will obviously arise -- for example, when multiple source
arguments are used on the command line. However. it's awkward to do in 
the general case, without repeating file scans on the server, which can
be very time-consuming. If in doubt, as always, use <code>--dry-run</code>
to check which files will be written.
<p/>
Using recursive mode (with <code>put</code>, <code>get</code>, and
<code>list</code>) can have unexpected results when the remote
path is not a directory. 
The reason for this is that file wildcards cannot be matched on the server 
-- the
entire, relevant directory tree has to be copied to the client first. 
The wildcard match is then done on the client. In recursive mode,
all files in the specified folder <i>and all subfolders</i> are 
searched on the client. If you run, for example,
<code>dbcmd list --recursive /foo.bar</code>, then the name 
<code>foo.bar</code> will be matched against every file on the server --
after transferring the complete file list to the client. More
useful, perhaps, is <code>dbcmd list --recursive /media/*.mp3</code>,
meaning "list every .mp3 file in every folder under <code>/media</code>. 
However,
even if this is potentially useful, even if it works it will
probably be slow and use a great deal of memory. 
<p/>
The Dropbox server is much more fussy than most Linux utilities are,
about file and folder paths. In particular, multiple forward-slashes
in a path name are treated as erroneous. 


<h2>Legal, etc</h2>

<code>dbcmd</code> is maintained by Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially, you may do
whatever you like with it, provided the original author is acknowledged, and
you accept the risks involved in doing so. <code>dbcmd</code> contains
contributions from a number of other authors, whose details may be
found in the source code.
<p/>

