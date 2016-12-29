# Gnutella
A Gnutella-like Simple Peer-to-Peer System

Project Statement: Develop a Gnutella-like simple peer-to-peer system
Project Objectives: Practicing socket programming and understanding the operations of peer-to-peer
systems, a popular file sharing paradigm on the Internet

Project Description:

In this project, you need to implement a Gnutella-like simple peer-to-peer system. To be precise, the
core protocol of the simple peer-to-peer system you need to develop is similar to the Gnutella protocol
version 0.4. For simplicity, we refer to the simple peer-to-peer system you need to develop as G0.4. In a
peer-to-peer system, each peer (node) is both a client and a server. A peer is a client in the sense that a
user can use it to search the peer-to-peer system and can retrieve files he is interested in from other
nodes in the system. A peer is a server in the sense that it can respond to search and download request
from other nodes. Such a peer (node) is often referred to as “servent”.
In G0.4, each node has a number of neighboring peers (or simply neighbors). The connections between
neighboring nodes (and connections for downloading files) are all “TCP” connections. Four types of
messages are supported on G0.4. In the following we briefly describe their functionalities to help us to
define the operations of the G0.4. The details of the messages will be described after we discuss the
operations of G0.4
*Ping: used by a node to discover new nodes in G0.4 so that it can connect to (to become
neighbors of each other)
*Pong: used by a node in response to an incoming Ping message to indicate that the node is
willing to accept a new neighbor
*Query: used by a node to search files on G0.4.
*QueryHit: used by a node in response to an incoming Query message if it hosts the
corresponding files.

In the following we describe the operations of the G0.4 peer-to-peer system, by focusing the operations
on a single node.

Starting a G0.4 node
When your program for a node starts (name your program as “g04”), it will first read a configuration file
under the same directory as the program g04. The name of the configuration file is “g04.conf”. This file
contains a few configuration parameters for your program, one line for each parameter. Each line of the
file is in the following format:
name=value

Where “name” is the name of the configuration parameter, and value is the corresponding value.
Required configuration parameters including the following (their details will be discussed when we need
them in the description of the protocol):

*neighborPort: port number on which the local node listens incoming TCP connection requests
for establishing connections with neighbors
*filePort: port number on which the local node listens incoming file transfer requests.
*NumberOfPeers: specifies the number of neighbors that this node should have. Before the node
obtains this number of neighbors, it will continue “ping” the G0.4 system to discover new
neighbors.
*TTL: maximum hops that a Ping or a Query message should be forwarded. After such a message
has traversed that maximum hops, the message should be discarded.
*seedNodes: a file containing seed nodes in the G0.4 system, through which the current node can
connect to G0.4 if it does not have any neighbors.
*isSeedNode: if the current node will serve as a seed node on G0.4 system. If a node is a seed
node, it will always accept incoming connection requests to add new neighbors. If a node is a
seed node, it will not actively probe the network to find a new neighbor. Instead, it will only
connect to the nodes given in the seedNodes file, if it is not empty.
*localFiles: a file containing names of files stored on the local node (to be shared with other
nodes on G0.4).

*localFilesDirectory: Directory where the local files are stored.
The file seedNodes contains address information of the seed nodes, which is given in the following
format: Each line contains the hostname (or IP address) and port number of a seed node. Hostname and
port number are separated by a white space.
The file localFiles contains all the information of files stored in the local store (localFileDirectory), each
line for a single file, with the following format:

*Filename: list of keywords separated by vertical bar
*Filename and search strings are case sensitive (technically, search strings do not need to case sensitive;

we simplify here to specify both are case sensitive. It is OK if you decide to only consider filename case
sensitive). One example line is:
Test.pl: Perl|Socket Programming|Test
The node needs to load in this file when the program starts. For simplicity, we assume this file will only
be edited by the user outside the G04 system (that is, the g04 program will not need to update this file
because of a new file being downloaded or an existing file is deleted).
When the program starts, it should create TCP socket on the two port numbers specified in the
configuration file (neighborPort and filePort) for accepting incoming connection requests for neighbors
or file downloads.

*Connecting to neighbors

After a node started, it will try to connect to the specified number of neighbors. First, the current node
will try to connect to one of the seed nodes given in the file seedNodes. After a connection has been
established with a seed node (see below on the details how a neighboring connection is considered
established), the current node will send out Ping message to try to discover more peers on the G0.4
system in order to have enough peers. If at least one neighbor is established, the current node will use it
to send out Ping messages, if not enough neighbors have been established. If no neighbors are
discovered (connected) after connecting to a seed node, the current node will move on to connect to
the next seed node. The same process as discussed above will be repeated periodically, until the
specified number of neighbors has been connected.

Now we describe how a neighboring connection is considered to be established with a remote peer.
First, the local node will establish a TCP connection with the remote peer. If a TCP connection can be
established, the following message will be sent to the remote peer:
GNUTELLA CONNECT/0.4\r\n
Where \r is the carriage-return character; and \n is the linefeed character.
If the remote peer can accept the local node as a new neighbor, the following message is sent back from
the remote peer:
GNUTELLA OK\r\n

A connection to a new neighbor is only established after the OK message is received. If the remote peer
cannot take the local node as a new neighbor (for example, because it already has enough neighbors),
the remote peer will just close the TCP connection without sending back the OK message. In the
following discussion, when we say a connection to a new neighbor is established, it means the local
node receives the OK message from the remote peer, that is, the two nodes become neighbors of each
other in the G0.4 system. (Establishing a TCP connection does not mean a connection to a new neighbor
has been established.)

After a node has enough neighbors, it may choose to disconnect from the seed node. If a current
neighbor is disconnected, the local node will connect to the next remote peer who has responded to
ping message, but have not been contacted. If no such remote peers are available, send ping message to
obtain more potential remote peers, and repeat what has been done above.
Handling Ping and Pong messages

As we have discussed above, when a node does not have enough neighbors, it will periodically send out
Ping messages, until enough neighbors have been established.

When a node receives an incoming Ping message, it will send out a Pong message if it still needs more
neighbors. It will decrease the TTL field of the Ping message by one (1), and forward the Ping message to
all its neighbors (except the neighbor from whom the Ping message is received), if the updated TTL is
still greater than zero.

When a node receives a Pong message, it will determine if the message is in response to its own Ping
message or in response to Ping message from a neighbor, based on the Message ID field of the Pong
message (see below). If the message is in response to its own Ping message, the node will process the
Pong message locally and try to connect to the specified remote peer contained in the Pong message, if
the current node still needs more neighbors. If the current node already has enough neighbors, it should
store the address information of the remote peer, which may be used later when an existing neighbor is
disconnected. If the message is in response to a Ping message from a neighboring node, the current
node will forward the Pong message further to the corresponding neighbor. Note that this means that a
node needs to “remember” all the ping messages that it has generated or forwarded, over a sufficient
long period of time.

Note: Duplicated connections between two neighboring nodes may be created in the current
specification. You do not need to worry about this problem. It is extremely unlikely to occur on the real
Gnutella system. Or you can extend the GNUTELLA CONNECT/0.4\r\n message to include the port
number of the local node: GNUTELLA CONNECT/0.4 port_number\r\n.
Search files on G04 system

A user can type the command “search” to search files on the G04 system. The search command takes
one argument, which is the keyword to be search. For example
G04: search Gnutella seed nodes

In response, the node will first search the local store to see if the local store contains any files matching
the keyword. If so, the name and information of the files will be displayed on the screen in the following
format:
Index size_in_Bytes_(or KB, or MB) filename location
Where “index” is an integer number referring to a file (starting from 0 for the first file, to N, the last file).
For files stored at the local store, show the “location” as local. For files stored at remote peers, show
“location” as the hostname or IP address of the corresponding peer hosting this file (see below).
In addition to searching the local store, the node will also send out “Query” messages to all neighbors.
When a node receives a Query message, it will check its local store, and send back a QueryHit message if
any of its files matches the keyword. It will update the TTL field of the corresponding Query message,
and forward the Query message to all its neighbors (except the one from which the Query message is
received), if the TTL value is still greater than zero.
When a node receives a QueryHit message, it will, similar to the handling of the Pong message, to
determine the QueryHit message is in response to a locally generated Query message, or a Query
message from one of the neighbors, based again on the Message ID associated with the messages. If it
is the former, the QueryHit message will be processed locally, and the files contained in the QueryHit
message will be displayed (note that, depending on how you code, the G0.4 prompt may be interleaved
with output due to the QueryHit messages, which is not a problem). If it is the latter case, the QueryHit
message will be further forwarded to the corresponding node from which the corresponding Query
message is received.

Reducing traffic:
A node should maintain a record of recently received (and processed or forwarded) Ping and Query
messages. When a duplicate Ping or Query message is received (it has identical “Message ID” and
“protocol ID” to a previous message received by the node), the node should not respond to or forward
this message again. The message can be safely discarded. Note that this method is not perfect in
reducing redundant traffic on G0.4, but it should work reasonably well.
Downloading a file
After a user gets a list of files related to the search keyword, the user can download a file directly from
the hosting node, by issuing a get command:
G04: get index [local_file_name]
Where “index” in the index value of a file in the list showing the files returned from the “search”
command; the optional “local_file_name” is the name of the file when it is stored on the local disk. If
“local_file_name” is not specified, you should use the original filename.
A node uses the HTTP protocol to download a file. After a TCP connection has been established to a
remote node hosting the desired file, the local node will send a HTTP request to the remote peer of the
following format:
GET /get/<File Name>/ HTTP/1.0\r\n
User-Agent: Gnutella\r\n
\r\n
Note that the pair of angle brackets <> is not part of the message you send. An example is as follows:
GET /get/Test.pl/ HTTP/1.0\r\n
User-Agent: Gnutella\r\n
\r\n
The remote peer will then respond with a HTTP message of the following format:
HTTP 200 OK\r\n
Server: Gnutella\r\n
Content-type: application/binary\r\n
Content-length: <size of file in bytes>\r\n
\r\n

The content of the file should immediately follow the last pair of \r\n. And the local node will read the
number of byte for the file content according to the value specified in the Content-length field of the
HTTP header. Similarly, <> surrounding “size of file in bytes” is not part of the message to send. You
should just put the file size there.
G0.4 Messages

In the following we will detail the format of the G0.4 messages. We will specify the fields of a message.
In addition, we will also specify the number of bytes for each field for “binary” message header. The
number of bytes should be the same as the original Gnutella protocol 0.4. However, we want to note
first that, you are not required to encode and transfer message headers in binary format. We provide
the information here in case you want to try the binary encoding and transfer of data over TCP. We
recommend you to do so in order to better understand the issues involved in transferring binary data;
however it is not required for this project.

The fields of common G0.4 Message header (used by all four types of messages, Ping, Pong, Query, and
QueryHit), given in the order in which they should be included in the message:
*Message ID: Unique message ID to identify this message in G0.4 system. This should be
generated using some random number generators. It is 16 bytes in length. For Message ID, it is
better for you to use exactly 16 bytes even if you will use text-based protocols.
*Protocol ID: what is the protocol (Ping, Pong, Query, or QueryHit)? 1 Byte.
*TTL: current value of TTL, updated at each immediate node. 1 Byte.
*Hops: number of nodes this message has traversed. 1 Byte. Each node increases this value by 1.
*Payload length: number of bytes in the payload (remainder) of the message. 4 bytes.
Ping Message:
*No payload. If the protocol ID is “Ping”, there is no payload.
Pong Message:
*Port: port number on which the remote peer (the sender of the pong message) will take
incoming connection request for creating neighbors. 2 types.
*IP address: IP address of the remote peer. 4 bytes.
*Number of files shared: Number of files that the remote node can share. 4 bytes
*Number of KB shared: Number of total file sizes in KB that the remote node can share. 4 bytes.
For this project, we do not use the two pieces of information: number of files and number of KB shared.
They are used by a local node to determine which remote peer to prefer to become neighbors. When
we say “we do not use a field”, we mean that you can still include this field in the message, but do not
need to process or respond to it. If you prefer, you can always use all fields.
Query Message:
*Minimum speed: minimum file downloading speed. 2 bytes. We do not use this field in our
project
*Search criteria: the null-terminated search string. Length is determined by the “Payload length”
field in the common message header.
QueryHit Message:
*Number of hits: Number of files matching the search criteria. 1 byte.
*Port: port number on which the remote peer (the sender of the message) will listen for incoming
file transfer request.
*IP address: IP address of the remote peer.
*Speed: downloading speed of the remote peer. We do not use this field in this project.
Result set: set of files satisfying the search criteria. There are “Number of hits” elements in this
set, each containing three fields:
o File Index: some local index of the file. 4 bytes. We do not use this field in this project.
o File Size: File size in number of bytes. 4 bytes.
o File name: double null-terminated file name. Variable length.
Servent ID: unique identifier of the remote peer (sender of the message) on G0.4 system. This is
normally some function of the IP address of the peer, for example, some hash of the IP address. 16
bytes. We do not use this field in this project.

References
1. The annotated Gnutella protocol specification v0.4.
http://rfc-gnutella.sourceforge.net/developer/stable/index.html
2. The Gnutella Protocol Specification v0.4. By Clip2 Distributed Search Services.
http://www.stanford.edu/class/cs244b/gnutella_protocol_0.4.pdf
