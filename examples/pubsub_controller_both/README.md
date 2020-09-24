PubSub NF and event interface in general
==

This README will try to explain how the current implementation of the event API works with onvm.

The API is all in the `onvm_nflib/onvm_event.h` file. Current implementation creates a tree in huge page memory to manage event to subscriber relations. Also current implementation uses a global root node pointer(defined by the pub sub controller) to simplify some operations, which can easily be changed to send messages to pubsub controller instead(this approach might be safer than a global pointer but some read operations should technically be okay, thus some consideration is required).

*I'm writting a high level overview with minimal implementation description as code is simple enough. Some things could be hacky as I was implementing this in limited time, fix them if needed. Feel free to ping me on slack with specific questions*

I have tried to be descriptive in the naming/commenting/todo requests, although names could defintelly be imrpoved.

The main functionality can be broken into 3 parts:

Pub Sub Controller
===
The NF initializes the initial main events and manages the event tree in shared memory. The NF serves such requests as SUBSCRIBE NF, PUBLISH EVENT and request global root pointer.


Publisher NF (f.e tcp stack)
===
Provided example showcasing how to publish new events. The assumption is all EVENT_IDs are defined in the shared header file. The publisher NF can check if the NF is subscribed to something and send out a message to that NF.


Subscriber NF (consumer)
===

The consumer will subscribe to different events and then process incoming messages and process those that it is subbed to and pass along others. This part can technically be easily improved, if we only have 1 callback per event type for NF (non flow specific) we can just add pointer to the event struct. That way the NF can retrieve it from the global easy (this is better because of recursive subscriptions as we would copy the callbacks to children of the event we subscribed to). Another option is to do this locally, although one has to decide how to deal with recursive subscription logic.


Running things
==

Here is what I used, mostly only the order matters due to hard coded instance IDs

onvm_mgr
`./go.sh 1,2,3,4 1 0xf20 -s stdout -a 0x7f000000000  -v`

pubsub controller
`./go.sh 1 -d 2`

 event_pub
`./go.sh 2 -d 3`

event_sub
`./go.sh 3 -d 4`
