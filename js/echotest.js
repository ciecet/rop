var trans = new Transport("10.12.0.7:9999", function() {
    var rr = trans.registry.getRemote("Echo")
    console.log(rr)
    var e = createStub("test.Echo", rr)
    var ec = {
        name: "test.EchoCallback",
        call: function(msg) {
            alert("got "+msg)
        }
    }

    alert(e.echo("한글테스트"))
    alert(e.concat(["수인", "현옥"]))
    try {
        e.touchmenot()
    } catch (ex) {
        inspect(ex)
    }

    e.recursiveEcho("Hello there!", ec)
    e.hello({name:"Sooin", callback:ec})
    e.asyncEcho("Hello Sooin!", ec)
})

/*
alert(e.echo("한글테스트"))
alert(e.concat(["수인", "현옥"]))
try {
    e.touchmenot()
} catch (ex) {
    inspect(ex)
}

e.recursiveEcho("Hello there!", ec)
e.hello({name:"Sooin", callback:ec})

alert("sending dispose")
e.dispose()
*/

//req = new XMLHttpRequest()
//req.open("POST", "http://192.168.10.3:8080", false)
//req.setRequestHeader("Content-Type", "text/plain")
//req.send("hi there!")
//alert("got "+req.responseText)
