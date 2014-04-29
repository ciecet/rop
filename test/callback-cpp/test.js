acba.define("js:aa", ["js:rop"], function(r) {
    var types = r.types
    var I8 = types.i8
    var I16 = types.i16
    var I32 = types.i32
    var I64 = types.i64
    var STRING = types.String
    var VOID = types.void
    var BOOLEAN = types.bool
    var F32 = types.f32
    var F64 = types.f64
    var VARIANT = types.Variant
    var NullableCodec = r.codecs.Nullable
    var ListCodec = r.codecs.List
    var MapCodec = r.codecs.Map
    var StructCodec = r.codecs.Struct
    var ExceptionCodec = r.codecs.Exception
    var InterfaceCodec = r.codecs.Interface
    types["test.Callback"] = [InterfaceCodec, undefined, ["run", [STRING], [VOID]]]
    types["test.Test"] = [InterfaceCodec, undefined, ["setCallback", ["test.Callback"], undefined, "echo", [STRING], [STRING]]]
    return types
})
