import java.util.*;

interface Echo {
    String echo (String msg);
    String concat (List words);
    void touchmenot () throws TestException;
    void recursiveEcho (String msg, EchoCallback cb);
    void hello (Person p);
    void asyncEcho (String msg, EchoCallback cb);
}
