.. _message-passing:

Message Passing
===============

The messaging layer of CAF has three primitives for sending messages: ``send``,
``request``, and ``delegate``. The former simply enqueues a message to the
mailbox of the receiver. The latter two are discussed in more detail in
:ref:`request` and :ref:`delegate`. Before we go into the details of the message
passing API itself, we first discuss the building blocks that enable message
passing in the first place.

.. _mailbox-element:

Structure of Mailbox Elements
-----------------------------

When enqueuing a message to the mailbox of an actor, CAF wraps the content of
the message into a ``mailbox_element`` (shown below) to add meta data
and processing paths.

.. _mailbox_element:

.. image:: mailbox_element.png
   :alt: UML class diagram for ``mailbox_element``

The sender is stored as a ``strong_actor_ptr`` (see :ref:`actor-pointer`) and
denotes the origin of the message. The message ID is either 0---invalid---or a
positive integer value that allows the sender to match a response to its
request. The ``stages`` vector stores the path of the message. Response
messages, i.e., the returned values of a message handler, are sent to
``stages.back()`` after calling ``stages.pop_back()``. This allows CAF to build
pipelines of arbitrary size. If no more stage is left, the response reaches the
sender. Finally, ``payload`` is the actual content of the message.

Mailbox elements are created by CAF automatically and are usually invisible to
the programmer. However, understanding how messages are processed internally
helps understanding the behavior of the message passing layer.

.. _copy-on-write:

Copy on Write
-------------

CAF allows multiple actors to implicitly share message contents, as long as no
actor performs writes. This allows groups (see :ref:`groups`) to send the same
content to all subscribed actors without any copying overhead.

Actors copy message contents whenever other actors hold references to it and if
one or more arguments of a message handler take a mutable reference.

Requirements for Message Types
------------------------------

Message types in CAF must meet the following requirements:

1. Inspectable (see :ref:`type-inspection`)
2. Default constructible
3. Copy constructible

A type ``T`` is inspectable if it provides a free function
``inspect(Inspector&, T&)`` or specializes ``inspector_access``.
Requirement 2 is a consequence of requirement 1, because CAF needs to be able to
create an object for ``T`` when deserializing incoming messages. Requirement 3
allows CAF to implement Copy on Write (see :ref:`copy-on-write`).

.. _special-handler:

Default and System Message Handlers
-----------------------------------

CAF has three system-level message types (``down_msg``, ``exit_msg``, and
``error``) that all actors should handle regardless of their current state.
Consequently, event-based actors handle such messages in special-purpose message
handlers. Additionally, event-based actors have a fallback handler for unmatched
messages. Note that blocking actors have neither of those special-purpose
handlers (see :ref:`blocking-actor`).

.. _down-message:

Down  Handler
~~~~~~~~~~~~~

Actors can monitor the lifetime of other actors by calling
``self->monitor(other)``. This will cause the runtime system of CAF to send a
``down_msg`` for ``other`` if it dies. Actors drop down messages unless they
provide a custom handler via ``set_down_handler(f)``, where ``f`` is a function
object with signature ``void (down_msg&)`` or
``void (scheduled_actor*, down_msg&)``. The latter signature allows users to
implement down message handlers as free function.

.. _exit-message:

Exit Handler
~~~~~~~~~~~~

Bidirectional monitoring with a strong lifetime coupling is established by
calling ``self->link_to(other)``. This will cause the runtime to send an
``exit_msg`` if either ``this`` or ``other`` dies. Per default, actors terminate
after receiving an ``exit_msg`` unless the exit reason is
``exit_reason::normal``. This mechanism propagates failure states in an actor
system. Linked actors form a sub system in which an error causes all actors to
fail collectively. Actors can override the default handler via
``set_exit_handler(f)``, where ``f`` is a function object with signature
``void (exit_message&)`` or ``void (scheduled_actor*, exit_message&)``.

.. _error-message:

Error Handler
~~~~~~~~~~~~~

Actors send error messages to others by returning an ``error`` (see
:ref:`error`) from a message handler. Similar to exit messages, error messages
usually cause the receiving actor to terminate, unless a custom handler was
installed via ``set_error_handler(f)``, where ``f`` is a function object with
signature ``void (error&)`` or ``void (scheduled_actor*, error&)``.
Additionally, ``request`` accepts an error handler as second argument to handle
errors for a particular request (see :ref:`error-response`). The default handler
is used as fallback if ``request`` is used without error handler.

.. _default-handler:

Default Handler
~~~~~~~~~~~~~~~

The default handler is called whenever the behavior of an actor did not match
the input. Actors can change the default handler by calling
``set_default_handler``. The expected signature of the function object
is ``result<message> (scheduled_actor*, message_view&)``, whereas the
``self`` pointer can again be omitted. The default handler can return a
response message or cause the runtime to *skip* the input message to allow
an actor to handle it in a later state. CAF provides the following built-in
implementations: ``reflect``, ``reflect_and_quit``,
``print_and_drop``, ``drop``, and ``skip``. The former
two are meant for debugging and testing purposes and allow an actor to simply
return an input. The next two functions drop unexpected messages with or
without printing a warning beforehand. Finally, ``skip`` leaves the
input message in the mailbox. The default is ``print_and_drop``.

.. _request:

Requests
--------

A main feature of CAF is its ability to couple input and output types via the
type system. For example, a ``typed_actor<result<int32_t>(int32_t)>``
essentially behaves like a function. It receives a single ``int32_t`` as input
and responds with another ``int32_t``. CAF embraces this functional take on
actors by simply creating response messages from the result of message handlers.
This allows CAF to match *request* to *response* messages and to provide a
convenient API for this style of communication.

.. _handling-response:

Sending Requests and Handling Responses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Actors send request messages by calling ``request(receiver, timeout,
content...)``. This function returns an intermediate object that allows an actor
to set a one-shot handler for the response message. Event-based actors can use
either ``request(...).then`` or ``request(...).await``. The former multiplexes
the one-shot handler with the regular actor behavior and handles requests as
they arrive. The latter suspends the regular actor behavior until all awaited
responses arrive and handles requests in LIFO order. Blocking actors always use
``request(...).receive``, which blocks until the one-shot handler was called.
Actors receive a ``sec::request_timeout`` (see :ref:`sec`) error message (see
:ref:`error-message`) if a timeout occurs. Users can set the timeout to
``infinite`` for unbound operations. This is only recommended if the receiver is
known to run locally.

In our following example, we use the simple cell actor shown below as
communication endpoint.

.. literalinclude:: /examples/message_passing/request.cpp
   :language: C++
   :start-after: --(rst-cell-begin)--
   :end-before: --(rst-cell-end)--

To showcase the slight differences in API and processing order, we implement
three testee actors that all operate on a list of cell actors.

.. literalinclude:: /examples/message_passing/request.cpp
   :language: C++
   :start-after: --(rst-testees-begin)--
   :end-before: --(rst-testees-end)--

Our ``caf_main`` for the examples spawns five cells and assign the initial
values 0, 1, 4, 9, and 16. Then it spawns one instance for each of our testee
implementations and we can observe the different outputs.

Our ``waiting_testee`` actor will always print:

.. code-block:: none

   cell #9 -> 16
   cell #8 -> 9
   cell #7 -> 4
   cell #6 -> 1
   cell #5 -> 0

This is because ``await`` puts the one-shots handlers onto a stack and
enforces LIFO order by re-ordering incoming response messages as necessary.

The ``multiplexed_testee`` implementation does not print its results in
a predicable order. Response messages arrive in arbitrary order and are handled
immediately.

Finally, the ``blocking_testee`` has a deterministic output again. This is
because it blocks on each request until receiving the result before making the
next request.

.. code-block:: none

   cell #5 -> 0
   cell #6 -> 1
   cell #7 -> 4
   cell #8 -> 9
   cell #9 -> 16

Both event-based approaches send all requests, install a series of one-shot
handlers, and then return from the implementing function. In contrast, the
blocking function waits for a response before sending another request.

Sending Multiple Requests
~~~~~~~~~~~~~~~~~~~~~~~~~

Sending the same message to a group of workers is a common work flow in actor
applications. Usually, a manager maintains a set of workers. On request, the
manager fans-out the request to all of its workers and then collects the
results. The function ``fan_out_request`` combined with the merge policy
``select_all`` streamlines this exact use case.

In the following snippet, we have a matrix actor ``self`` that stores worker
actors for each cell (each simply storing an integer). For computing the average
over a row, we use ``fan_out_request``. The result handler passed to ``then``
now gets called only once with a ``vector`` holding all collected results. Using
a response promise promise_ further allows us to delay responding to the client
until we have collected all worker results.

.. literalinclude:: /examples/message_passing/fan_out_request.cpp
   :language: C++
   :start-after: --(rst-fan-out-begin)--
   :end-before: --(rst-fan-out-end)--

The policy ``select_any`` models a second common use case: sending a
request to multiple receivers but only caring for the first arriving response.

.. _error-response:

Error Handling in Requests
~~~~~~~~~~~~~~~~~~~~~~~~~~

Requests allow CAF to unambiguously correlate request and response messages.
This is also true if the response is an error message. Hence, CAF allows to add
an error handler as optional second parameter to ``then`` and ``await`` (this
parameter is mandatory for ``receive``). If no such handler is defined, the
default error handler (see :ref:`error-message`) is used as a fallback in
scheduled actors.

As an example, we consider a simple divider that returns an error on a division
by zero. This examples uses a custom error category (see :ref:`error`).

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :start-after: --(rst-divider-begin)--
   :end-before: --(rst-divider-end)--

When sending requests to the divider, we can use a custom error handlers to
report errors to the user like this:

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :start-after: --(rst-request-begin)--
   :end-before: --(rst-request-end)--

.. _delay-message:

Delaying Messages
-----------------

Messages can be delayed by using the function ``delayed_send``, as illustrated
in the following time-based loop example.

.. literalinclude:: /examples/message_passing/dancing_kirby.cpp
   :language: C++
   :start-after: --(rst-delayed-send-begin)--
   :end-before: --(rst-delayed-send-end)--

Delayed send schedules messages based on relative timeouts. For absolute
timeouts, use ``scheduled_send`` instead.

.. _delegate:

Delegating Messages
-------------------

Actors can transfer responsibility for a request by using ``delegate``.
This enables the receiver of the delegated message to reply as usual---simply
by returning a value from its message handler---and the original sender of the
message will receive the response. The following diagram illustrates request
delegation from actor B to actor C.

.. code-block:: none

                  A                  B                  C
                  |                  |                  |
                  | ---(request)---> |                  |
                  |                  | ---(delegate)--> |
                  |                  X                  |---\
                  |                                     |   | compute
                  |                                     |   | result
                  |                                     |<--/
                  | <-------------(reply)-------------- |
                  |                                     X
                  X

Returning the result of ``delegate(...)`` from a message handler, as
shown in the example below, suppresses the implicit response message and allows
the compiler to check the result type when using statically typed actors.

.. literalinclude:: /examples/message_passing/delegating.cpp
   :language: C++
   :start-after: --(rst-delegate-begin)--
   :end-before: --(rst-delegate-end)--

.. _promise:

Response Promises
-----------------

Response promises allow an actor to send and receive other messages prior to
replying to a particular request. Actors create a response promise using
``self->make_response_promise<Ts...>()``, where ``Ts`` is a
template parameter pack describing the promised return type. Dynamically typed
actors simply call ``self->make_response_promise()``. After retrieving
a promise, an actor can fulfill it by calling the member function
``deliver(...)``, as shown in the following example.

.. literalinclude:: /examples/message_passing/promises.cpp
   :language: C++
   :start-after: --(rst-promise-begin)--
   :end-before: --(rst-promise-end)--

This example is almost identical to the example for delegating messages.
However, there is a big difference in the flow of messages. In our first
version, the worker (C) directly responded to the client (A). This time, the
worker sends the result to the server (B), which then fulfills the promise and
thereby sends the result to the client:

.. code-block:: none

                  A                  B                  C
                  |                  |                  |
                  | ---(request)---> |                  |
                  |                  | ---(request)---> |
                  |                  |                  |---\
                  |                  |                  |   | compute
                  |                  |                  |   | result
                  |                  |                  |<--/
                  |                  | <----(reply)---- |
                  |                  |                  X
                  | <----(reply)---- |
                  |                  X
                  X

Message Priorities
------------------

By default, all messages have the default priority, i.e.,
``message_priority::normal``. Actors can send urgent messages by setting the
priority explicitly: ``send<message_priority::high>(dst, ...)``. Urgent messages
are put into a different queue of the receiver's mailbox. Hence, long wait
delays can be avoided for urgent communication.
