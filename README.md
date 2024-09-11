The simppl/dbus C++ library provides a high-level abstraction layer
for DBus messaging by using solely the C++ language and compiler for
the definition of remote interfaces. No extra tools are necessary to
generate the binding glue code.

simppl/dbus can interact with standard DBus services and also provides
support for the DBus.Properties and DBus.ObjectManager APIs. See the
examples subfolder on how to interact with standard Linux system services
like WPA supplicant or NetworkManager.


## Setup

For building the library and tests from source code you need cmake and
gtest. I use cmake 2.8 and gtest-1.7. You may need some changes in the
setup for environment variables in the contributed script file update.sh.
Development libraries of libdbus-1 are also needed for a successful build.


## Compilers

I only developed on GCC >=4.9. The code uses features of upto C++14 and
g++ specific demangling functions for class symbols. Therefore, I do not
expect the code to work on any other compiler family without appropriate
code adaptions.

For structure serialization boost::fusion can be used. See benchmark example
for more information on how to serialize structures when sending remote messages.
The boost way is the formal and correct way to serialize structures. The old
way of defining serializer_type's typdefs within structures is no longer
recommended since that one makes heavy use of C++ language to memory
mappings and will definitely not work with complex data types containing
virtual functions or containing multiple inheritence.


## Status

The library is used in a production project but it still can be
regarded as 'proof of concept'. Interface design may still be under change.
Due to the simplicity of the glue layer and the main functionality provided
by libdbus I would recommend to give it a try in real projects.


## First steps

The standard way to access DBus services is typically through using libdbus
or GDBus. While the APIs are very powerful, they do not provide an
adequate level of abstraction which could be provided by code generation
from DBus interface definitions, i.e. the DBus introspection XML.
simppl/dbus provides such an abstraction with the help of interface
descriptions written entirely in C++. This results is service APIs
which just looks like ordinary C++ function calls, on both sides, clients
(aka. stub) and servers (aka. skeletons).

The easiest way to implement services is to let simppl generate
bus names and object paths for your service interface definitions.


### Defining an interface


Let's start with a simple echo service. See the service description:

```c++
   namespace test
   {

   INTERFACE(EchoService)
   {
      Method<in<std::string>, out<std::string>> echo;
   };

   }   // namespace
```

Remember, this is pure C++. You see a method named 'echo' taking a
string as 'in' argument and a string as 'out' argument.
The header file with this definition can be included by the server and
by the client. A complete interface definition also needs some hand-written
glue code, but it's very simple. The full interface looks like this:


```c++
   namespace test
   {

   INTERFACE(EchoService)
   {
      Method<in<std::string>, out<std::string>> echo;

      // constructor
      EchoService()
       : INIT(echo)
      {
      }
   };

   }   // namespace
```

Now the interface is complete C++. You need to implement the constructor
to correctly initialize the members. For oneway methods, you just add
the keyword oneway to the method definition. oneway, in and out are
part of the C++ namespace simppl::dbus.

But, how is the service mapped to dbus? Well, have you seen the namespace
definition? Then you can already imagine how the service is mapped, can't you?
Services are typically instantiated and each instance will have its own
instance name. For now, let's say the instance name was 'myEcho'.
With this information provided in the constructor of the service instance,
the dbus bus name requested will be

```
   test.EchoService.myEcho
```

The objectpath of the service is build accordingly so the service is
provided under

```
   /test/EchoService/myEcho
```

This mapping is done by simppl automatically. But, as mentioned earlier,
you may connect your DBus object to any bus name and object path layout
in order to connect any available DBus service. Also, a server object
instance may provide multiple interfaces. But we ignore these details
for now and continue with our EchoService. Let's implement the
server by inheriting our implementation class from simppl's
skeleton, giving the skeleton the interface definition from above:


```c++
   class MyEcho : simppl::dbus::Skeleton<EchoService>
   {
      MyEcho(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<EchoService>(disp, "myEcho")
      {
      }
   };
```

This service instance does not implement a handler for the echo method
yet. But you have seen how the service's instance name is provided.
Let's provide an implementation for the echo method:


```c++
   class MyEcho : simppl::dbus::Skeleton<EchoService>
   {
      MyEcho(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<EchoService>(disp, "myEcho")
      {
         echo >> [](const std::string& echo_string)
         {
            std::cout << "Client says '" << echo_string << "'" << std::endl;
         };
      }
   };
```

Now, simppl will receive the echo request, unmarshall the argument and
forward the request to the provided lambda function connected via the
right shift operator. Instead of lambda functions, you may of course
use any other method to bind to a std::function with an appropriate
call interface, e.g. via using std::bind.

But there is still no response yet. Let's send a response back to the client:


```c++
   class MyEcho : simppl::dbus::Skeleton<EchoService>
   {
      MyEcho(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<EchoService>(disp, "myEcho")
      {
         echo >> [this](const std::string& echo_string)
         {
            std::cout << "Client says '" << echo_string << "'" << std::endl;
            respond_with(echo(echo_string));
         };
      }
   };
```

That's simppl, isn't it? simppl is designed to even provide asynchronous
responses with a call to defer_response. For more information, see examples
provided in the unittests.

Setup the eventloop and the server is finished:


   ```c++
   int main()
   {
      simppl::dbus::Dispatcher disp("bus:session");
      MyEcho instance(disp);

      disp.run();

      return EXIT_SUCCESS;
   }
```

Now it's time to implement a client. Clients are implemented by instantiation
of a stub. One can either write blocking clients which is only recommended
for simple tools or write a full featured event-driven client. Let's start
with a simple blocking client:


```c++
   int main()
   {
      simppl::dbus::Dispatcher disp("bus:session");

      simppl::dbus::Stub<EchoService> stub(disp, "myEcho");

      std::string echoed = stub.echo("Hello world!");

      return EXIT_SUCCESS;
   }
```

That's it. The method call blocks until the response is received and
the result is stored in the echoed variable. In case of an error, an exception is thrown.
Note that the underlying DBus implementation will block on the fd correlated
with the request and therefore a blocking client shall never be running on the
same connection as server objects, which could easily be mixed up in
more complex distributed services.

Important notice: simppl currently always
adds a signal filter to the message bus. Therefore, connections instantiated
for blocking clients and therefore never running any kind of event loop, will cause
the bus daemon to grow in size, as these bus clients will never read these signal
notifications from the bus.

But let's have a look on the message loop driven client. The best way to implement
such a client is to derive from the stub base template and delegate the
callbacks to member functions. Note that the method is called via the async(...)
member function.


```c++
   class MyEchoClient : simppl::dbus::Stub<EchoService>
   {
      MyEchoClient(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Stub<EchoService>(disp, "myEcho")
      {
         connected >> std::bind(&MyEchoClient::handle_connected, this, _1);
      }

      void handle_connected(simppl::dbus::ConnectionState st)
      {
         if (st == simppl::dbus::ConnectionState::Connected)
         {
            echo.async("Hello World!") >> [](const simppl::dbus::Callstate st, const std::string& echo_string)
            {
               if (st)
               {
                  std::cout << "Server says '" << echo_string << "'" << std::endl;
                  respond_with(echo(echo_string));
               }
               else
                  std::cout << "Got error: " << st.what() << std::endl;
            };
         }
      }
   };
```

Event loop driven clients always get callbacks called from the dbus runtime
for any occuring DBus event. The initial event for a client is the connected event
which will be emitted as soon as the server registers itself on the bus (remember
the busname). After being connected, the client may start an
asynchronous method like in the example above. The method response callbacks
can be any function object fullfilling the response signature, the preferred
way nowadays is a C++ lambda function. The main program is as simple as in the
blocking example:


```c++
   int main()
   {
      simppl::dbus::Dispatcher disp("bus:session");

      MyEchoClient client(disp);

      disp.run();

      return EXIT_SUCCESS;
   }
```

Easy, isn't it? But with simppl/dbus it is also possible to model signals
and properties. Moreover, any complex data can be passed between client
and server. See the following example:


```c++
   namespace test
   {
      struct Data
      {
         typedef make_serializer<int, std::string, std::vector<std::tuple<int, double>>>::type serializer_type;

         int i;
         std::string str;
         std::vector<std::tuple<int, double>> vec;
      };


      INTERFACE(ComplexTest)
      {
         Method<in<Data>, out<int>, out<std::string>> eval;

         Signal<std::map<int,int>> sig;

         Property<Data> data;

         ComplexTest()
          : INIT(eval)
          , INIT(sig)
          , INIT(data)
         {
         }
      };
   }
```

The Data structure is any C/C++ struct but must not include virtual functions.
For structures with a more complex memory layout it is adviced to use boost::fusion
to map the struct to a template-iteratable type sequence. For simpler types
like above, the make_serializer generates an adequate serialization code for
the structure, i.e. the structure can be used as any simple data type:


```c++
   int main()
   {
      simppl::dbus::Dispatcher disp("bus:session");

      simppl::dbus::Stub<ComplexTest> teststub(disp, "test");

      Data d({ 42, "Hallo", ... });

      int i_ret;
      std::string s_ret;

      std::tie(i_ret, s_ret) = teststub.eval(d);

      return EXIT_SUCCESS;
   }
```

Have you noticed how methods with more than one return value are mapped
to a tuple out parameter which can be tie'd to the local variables in
this blocking call? Isn't that cool?

The signal in the example above is only sensefully usable
in an event loop driven client. There is a slight difference between
the properties concepts of simppl and dbus for now: the changed notification
will not be part of the Properties interface so only get and set may
currently be used to access properties of non-simppl services. But this is
nothing that cannot be changed in future. See how a client will typically
register for update notifications in order to receive changes on the properties
on server side. See the clients connected callback:


```c++
   class MyComplexClient : simppl::dbus::Stub<ComplexTest>
   {
      MyComplexClient(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Stub<ComplexTest>(disp, "test")
      {
         connected >> [this](simppl::dbus::ConnectionState st)
         {
            if (st == simppl::dbus::ConnectionState::Connected)
            {
               sig.attach() >> [](const std::map<int,int>& m)
               {
                  ...
               };

               data.attach() >> [](const Data& d)
               {
                  // first callback is due to the attach!
                  ...
               };
            }
            else
            {
               sig.detach();
               data.detach();
            }
         };
      }
   };
```

Properties on server side can be either implemented by providing a
callback function to be called whenever the property is requested by a
client or the property value can be stored within the server instance.
Therefore, you have to decide and initialize the property within the
server's constructor. Let's see the difference:

```c++
   class MyComplexServer : simppl::dbus::Skeleton<ComplexTest>
   {
      MyComplexServer(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<ComplexTest>(disp, "test")
      {
         // either using the callback version...
         data.on_read([](){
            return Data({ 42, "Hallo", ... });
         });

         // ... or keep a copy in the server instance
         data = Data({ 42, "Hallo", ... });
      }
   };
```

The notification of property changes is either done by calling the properties
notify(...) method in case the property is initialized for callback access
or by just assigning a new value to the stored property instance.
The version of property access to be used in your server is completely
transparent for the client and depends on your use-case.

Simppl properties also support the GetAll interface. See unittest for an example.

User-defined exceptions may also be transferred. This feature is currently
restricted to method calls. To define an error you have to use the boost
fusion binding:

```c++
   namespace test
   {

   class MyError : simppl::dbus::Error
   {
   public:

      // needed for client side code generation
      MyError() = default;

      // used to throw error on server side
      MyError(int rc)
       : simppl::dbus::Error("My.Error")   // make a DBus appropriate error name
       , result(rc)
      {
         // NOOP
      }

      int result;
   };

   }   // namespace

   BOOST_FUSION_ADAPT_STRUCT(
      test::MyError,
      (int, result)
   )
```

In the interface definition the exception class has to be added:

```c++
   namespace test
   {

   INTERFACE(HelloService)
   {
      Method<in<std::string>, _throw<MyError>> hello;

      // constructor
      HelloService()
       : INIT(hello)
      {
      }
   };

   }   // namespace
```

You may now throw the error in a server's method callback:

```c++
   class MyHello : simppl::dbus::Skeleton<HelloService>
   {
      MyHello(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<HelloService>(disp, "myHello")
      {
         hello >> [](const std::string& hello_string)
         {
            if (hello_string == "idiot")
                respond_with(MyError(EINVAL));

            respond_with(hello());
         };
      }
   };
```

This was a short introduction to simppl/dbus. I hope you will like
developing client/server applications with the means of C++ and without
the need of a complex tool chain for glue-code generation.


## Lifetime tracking

In the beginning, simppl was designed without DBus as the transport layer in mind.
Therefore, the connected/disconnected handling  of the clients currently only
works if the server binary will exit, i.e. the DBus connection hold to the
bus daemon will be closed. Otherwise, the bus names will not be released
and therefore, clients will not get notified when a server object was destroyed.
A more robust solution for this topic is to make use of the DBus ObjectManager
interface, which will inform you whenever DBus objects are being added or
removed from an object path instead of the bus name.

This can be achieved by simply creating a server object which implements
the standard DBus.ObjectManager interface:

```c++
   #define SIMPPL_HAVE_OBJECTMANAGER 1

   #include "simppl/...>

   simppl::dbus::Skeleton<org::freedesktop::DBus::ObjectManager> mgr(s, "s");

   simppl::dbus::Skeleton<test::One> o(s, "s");
   o.Str = "Hallo";
   o.Int = 42;

   mgr.add_managed_object(&o);
```

ObjectManager support may be disabled for compatibility reasons in cmake.
Beware, that client code has to set the macro to the same value as during
library compilation.


## Eventloop integration

The libdbus reference implementation allows the integration of the DBus
API in any thirdparty event loop. You may access the DBusConnection object
from the simppl::dbus::Dispatcher in order to integrate simppl in your
project's event loop. For an example implementation using boost::asio
see the examples source code in the asio subdirectory.
