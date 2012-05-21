Types.Person = [StructCodec,
    STRING, "name",
    "EchoCallback", "callback",
]
Types.TestException = [ExceptionCodec, "TestException",
    [NullableCodec, I32], "i"
]
Types.EchoCallback = [InterfaceCodec,
    "call", [STRING], undefined // async
]
Types.Echo = [InterfaceCodec,
    "echo", [STRING], [STRING],
    "concat", [[ListCodec, STRING]], [STRING],
    "touchmenot", undefined, [VOID, "TestException"],
    "recursiveEcho", [STRING, "EchoCallback"], [VOID],
    "hello", ["Person"], [VOID]
]

var reg = new Registry()
reg.setTransport(new Transport("http://10.12.0.7:8080"))
var rr = reg.getRemote("Echo")
var e = createStub("Echo", rr)
alert(e.echo("한글테스트"))
alert(e.concat(["수인", "현옥"]))
try {
    e.touchmenot()
} catch (ex) {
    inspect(ex)
}
var ec = {
    name: "EchoCallback",
    call: function(msg) {
        alert("got "+msg)
    }
}

e.recursiveEcho("Hello there!", ec)
e.hello({name:"Sooin", callback:ec})

alert("sending dispose")
e.dispose()

//req = new XMLHttpRequest()
//req.open("POST", "http://192.168.10.3:8080", false)
//req.setRequestHeader("Content-Type", "text/plain")
//req.send("hi there!")
//alert("got "+req.responseText)
