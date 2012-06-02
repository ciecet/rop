class RDParser
    attr_accessor :pos
    attr_reader :rules

    def initialize(&block)
        @lex_tokens = []
        @rules = {}
        @start = nil
        instance_eval(&block)
    end

    def parse(string)
        @tokens = []
        until string.empty?
            raise "unable to lex '#{string}" unless @lex_tokens.any? do |tok|
                match = tok.pattern.match(string)
                
                if match
                    @tokens << tok.block.call(match.to_s) if tok.block
                    string = match.post_match
                    true
                else
                    false
                end
            end
        end
        @pos = 0
        @max_pos = 0
        @expected = []
        result = @start.parse
        #puts "pos:#{@pos} tokens:#{@tokens.inspect} maxpos:#{@max_pos}"
        if @pos != @tokens.size
            raise "Parse error. expected: '#{@expected.join(', ')}', found '#{@tokens[@max_pos]}'"
        end
        return result
    end

    def next_token
        @pos += 1
        return @tokens[@pos - 1]
    end

    def expect(tok)
        t = next_token
        if @pos - 1 > @max_pos
            @max_pos = @pos - 1
            @expected = []
        end
        return t if tok === t
        @expected << tok if @max_pos == @pos - 1 && !@expected.include?(tok)
        return nil
    end

    private

    LexToken = Struct.new(:pattern, :block)

    def token(pattern, &block)
        @lex_tokens << LexToken.new(Regexp.new('\\A(?:' + pattern.source+')', pattern.options), block)
    end

    def start(name, &block)
        rule(name, &block)
        @start = @rules[name]
    end

    def rule(name)
        @current_rule = Rule.new(name, self)
        @rules[name] = @current_rule
        yield
        @current_rule = nil
    end

    def match(*pattern, &block)
        @current_rule.add_match(pattern, block)
    end

    class Rule
        Match = Struct.new :pattern, :block

        def initialize(name, parser)
            @name = name
            @parser = parser
            @matches = []
            @lrmatches = []
        end

        def add_match(pattern, block)
            match = Match.new(pattern, block)
            if pattern[0] == @name
                pattern.shift
                @lrmatches << match
            else
                @matches << match
            end
        end

        def parse
            #puts "matches:#{@matches.inspect}"
            match_result = try_matches(@matches)
            #puts "result:#{match_result.inspect}"
            return nil unless match_result
            loop do
                result = try_matches(@lrmatches, match_result)
                return match_result unless result
                match_result = result
            end
        end

        private

        def try_matches(matches, pre_result = nil)
            match_result = nil
            start = @parser.pos
            matches.each do |match|
                r = pre_result ? [pre_result] : []
                match.pattern.each do |token|
                    #puts "for token:#{token}"
                    if @parser.rules[token]
                        r << @parser.rules[token].parse
                        unless r.last
                            r = nil
                            break
                        end
                    else
                        nt = @parser.expect(token)
                        if nt
                            r << nt
                        else
                            r = nil
                            break
                        end
                    end
                end
                if r
                    if match.block
                        match_result = match.block.call(*r)
                    else
                        match_result = r[0]
                    end
                    break
                else
                    @parser.pos = start
                end
            end
            return match_result
        end
    end
end

=begin

packages: [packages] package <name> { definitions }
definitions: definitions | structdef | interfacedef
structdef: struct <name> { fields }
fields: fields field | field
field: type <var>;
type: Map<type,type> | List<type> | Nullable<type> | struct_name | interface_name
interfacedef: interface <name> { methods }
methods: methods method
method: return_type <method> args; | sync_return_type <method> args throws Exceptions;
return_type: sync_return_type | async
args: () | (arglist)
arglist: arglist, arg | arg
arg: type <var>

=end

PackageNode = Struct.new(:name, :defs, :structs, :exceptions, :interfaces)
StructNode = Struct.new(:name, :fieldTypes)
ExceptionNode = Struct.new(:name, :fieldTypes)
InterfaceNode = Struct.new(:name, :methods)
MethodNode = Struct.new(:name, :returnTypes, :argumentTypes)
TypeNode = Struct.new(:name, :subTypes, :variable, :package)
class TypeNode 
    def check p
        self.package = p
        if subTypes
            subTypes.each { |st|  
                st.check p
            }
        end

        return if %w(void async String Map List Nullable i8 i16 i32).include?(name)
        return if package.defs.has_key?(name)
        throw "Unknown type #{name}"
    end
end

NAME = /[a-zA-Z_][a-zA-Z0-9_]*/

IDLParser = RDParser.new do
    token(/\b(package|struct|exception|interface)\b/) { |r| r.intern }
    token(/[<>{}();,]/) { |s| s }
    token(NAME) { |s| s }
    token(/\s*/)

    def parse string
        packages = super
        packages.each { |p|
            p.structs = {}
            p.exceptions = {}
            p.interfaces = {}
            p.defs.each { |n,d|
                case d
                when StructNode
                    p.structs[n] = d
                when InterfaceNode
                    p.interfaces[n] = d
                when ExceptionNode
                    p.exceptions[n] = d
                end
            }
        }

        packages.each { |p|

            p.structs.each { |_,s|
                s.fieldTypes.each { |t|
                    t.check p
                }
            }
            p.exceptions.each { |_,e|
                e.fieldTypes.each { |t|
                    t.check p
                }
            }
            p.interfaces.each { |_,intf|
                intf.methods.each { |m|
                    m.returnTypes.each { |t|
                        t.check p
                    }
                    m.returnTypes[1..-1].each { |t|
                        throw "Non exception type #{t}" unless p.exceptions.has_key?(t.name)
                    }
                    m.argumentTypes.each { |t|
                        t.check p
                    }
                }
            }
        }
        return packages
    end

    start :packages do
        match(:package, NAME, "{", :definitions, "}") { |a,b,c,d,e|
                [ PackageNode.new(b, d) ] }
        match(:packages, :package, NAME, "{", :definitions, "}") { |p,a,b,c,d,e|
                p << PackageNode.new(b,d) }
    end

    rule :definitions do
        match(:definition) { |a| {a.name => a} }
        match(:definitions, :definition) { |a,b| a[b.name] = b; a }
    end

    rule :definition do
        match(:struct, NAME, "{", :fields, "}") { |a,b,c,d,e|
                StructNode.new(b,d)}
        match(:exception, NAME, "{", :fields, "}") {|a,b,c,d,e|
                ExceptionNode.new(b,d)}
        match(:interface, NAME, "{", :methods, "}") { |a,b,c,d,e|
                InterfaceNode.new(b,d)}
    end

    rule :fields do
        match(:field) { |a| [a] }
        match(:fields, :field) { |a,b| a+[b] }
    end

    rule :field do
        match(:type, NAME, ";") {|a,b,c| a.variable = b; a}
    end

    rule :type do
        match("Map", "<", :type, ",", :type, ">") { |a,b,c,d,e,f|
                TypeNode.new(a, [c, e])}
        match("List", "<", :type, ">") { |a,b,c,d|
                TypeNode.new(a, [c])}
        match("Nullable", "<", :type, ">") {|a,b,c,d|
                TypeNode.new(a, [c])}
        match(NAME) { |a| TypeNode.new(a) }
    end

    rule :methods do
        match(:method) { |a| [a] }
        match(:methods, :method) { |a,b| a+[b] }
    end

    rule :method do
        match(:returnType, NAME, :args, ";") { |a,b,c,d|
                MethodNode.new(b, [a], c)}
        match(:syncReturnType, NAME, :args, "throws", :exceptions, ";") {
                |a,b,c,d,e,f| MethodNode.new(b, [a]+e, c)}
    end

    rule :returnType do
        match(:syncReturnType)
        match(:asyncReturnType)
    end

    rule :syncReturnType do
        match(:type)
        match("void") { |a| TypeNode.new(a) }
    end

    rule :asyncReturnType do
        match("async") { |a| TypeNode.new(a) }
    end

    rule :args do
        match("(", ")") { [] }
        match("(", :arglist, ")") { |a,b,c| b }
    end

    rule :arglist do
        match(:arg) { |a| [a] }
        match(:arglist, ",", :arg) {|a,b,c| a+[c]}
    end

    rule :arg do
        match(:type, NAME) {|a,b| a.variable = b; a}
    end

    rule :exceptions do
        match(NAME) { |a| [TypeNode.new(a)] }
        match(:exceptions, ",", NAME) { |a,b,c| a+[TypeNode.new(c)] }
    end
end

