Types["test.Person"] = [StructCodec, STRING, "name", "test.EchoCallback", "callback"]
Types["test.TestException"] = [ExceptionCodec,"test.TestException", [NullableCodec, I32], "i"]
Types["test.EchoCallback"] = [InterfaceCodec, "call", [STRING], undefined]
Types["test.Echo"] = [InterfaceCodec, "echo", [STRING], [STRING], "concat", [[ListCodec, STRING]], [STRING], "touchmenot", undefined, [VOID, "test.TestException"], "recursiveEcho", [STRING, "test.EchoCallback"], [VOID], "hello", ["test.Person"], [VOID], "asyncEcho", [STRING, "test.EchoCallback"], undefined]
