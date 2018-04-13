The simppl/dbus C++ library provides a high-level abstraction layer
for DBus messaging by using solely the C++ language and compiler for
the definition of remote interfaces. No extra tools are necessary to
generate the binding glue code.


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
for more information how to serialize structures when sending remote messages.
The boost way is the formal and correct way to serialize structures. The old
way of defining serializer_type's typdefs within structures is no longer
recommended since that one makes heavy use of C++ language to memory
mappings and will definitely not work with complex data types containing
virtual functions or containing multiple inheritence.


## Status

The library is used in a production project and but it still can be
regarded as 'proof of concept'. Interface design may still be under change.
Due to the simplicity of the glue layer and the main functionality provided
by libdbus I would recommend to give it a try in real projects.


## CopyLeft

Feel free to do with the code whatever you want. It's free open-source code.


## First steps

DBus uses an XML document for interface definition. This is rather tricky
to read and typically source code is generated in order to provide
these interfaces in the native programming language of a project.
simppl/dbus does provide a binding written entirely in C++ itself.
DBus provides many possibilities to map services (interfaces) to busnames
and objectpaths. Simppl currently makes some assumptions and therefore
looses some generality which is no problem for pure simppl projects but
currently may provide trouble with accessing existing services which do
not share these simplifications. But let's go on and see how a simppl
service is defined. We will recall these facts later.


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

Now the interface is complete C++. You need the implement the constructor
to correctly initialize the members. For oneway methods, you just add
the keyword oneway to the method definition. oneway, in and out are
part of the C++ namespace simppl::dbus.

But, how is the service mapped to dbus? Well, have you seen the namespace
definition? Then you can already imagine how the service is mapped, can't you?
Services are typically instantiated and each instance will have its own
instance name. For now, let's say the instance name was 'myEcho'.
With this information provided in the constructor of the service instance,
the dbus busname requested will be

```
   test.EchoService.myEcho
```

The objectpath of the service is build accordingly so the service is
provided under

```
   /test/EchoService/myEcho
```

This mapping is done automatically by simppl, for servers this is currently
fix. But clients may connect to any bus/objectpath layout in order to connect
any DBus service. This also means that only one interface can be provided 
by a distinct objectpath, at least other than the properties interface needed for
providing service properties. But we will ignore signals and
properties for now and continue with our EchoService. Let's instantiate the
service server by inheriting our implementation class from simppl's
Skeleton:


```c++
   class MyEcho : simppl::dbus::Skeleton<EchoService>
   {
      MyEcho(simppl::dbus::Dispatcher& disp)
       : simppl::dbus::Skeleton<EchoService>(disp, "myEcho")
      {
      }
   };
```

This service instance did not implement a handler for the echo method
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
forward the request to the provided lambda function (of course, you may 
also bind any other function via std::bind). But there is still
no response yet. Let's send a response back to the client:


```c++
   class MyEcho : simppl::dbus::Skeleton<EchoService>
   {
      MyEcho()
       : simppl::dbus::Skeleton<EchoService>("myEcho")
      {
         echo >> [this](const std::string& echo_string)
         {
            std::cout << "Client says '" << echo_string << "'" << std::endl;
            respond_with(echo(echo_string));
         };
      }
   };
```

That's simppl, isn't it? Setup the eventloop and the server is finished:


   ```c++
   int main()
   {
      simppl::dbus::Dispatcher disp("bus::session");
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
stored in the echoed variable. In case of an error, an exception is thrown.
Note that the underlying DBus implementation will block on the fd correlated
with the request and therefore a blocking client can never be running on the
same connection as the server (ok, that's only a test setup, in real world
client and server typically reside in different applications.

But have a look on the message loop driven client. The best way to implement
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
when any event occurs. The initial event for a client is the connected event
which will be emitted when the server registers itself on the bus (remember
the busname). After being connected, the client may start an
asynchronous method like in the example above. The method response callbacks 
can be any function object fullfilling the response signature, the preferred
way nowadays in a C++ lambda. The main program is as simple as in the
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
For structures with complexer memory layout it is adviced to use boost::fusion
to map the struct to a template-iteratable type sequence. For simpler types
like above, the make_serializer generates an adequate serializing code for
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
       : simppl::dbus::Stub<ComplexTest>(disp, "myEcho")
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

This was a short introduction to simppl/dbus. I hope you will like
developing client/server applications with the means of C++ and without
the need of a complex tool chain for glue-code generation.
