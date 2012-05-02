#ifndef STACK_H
#define STACK_H

#define STACK_SIZE 1024

enum RESULT { STOPPED, CONTINUE, COMPLETE };

struct Frame {
    Frame *up;
    int step;

    Frame (): step(0) {}

    virtual RESULT run (Stack *stack) { return COMPLETE; }
    virtual ~Frame() {};
};

struct Stack {
    void *env;
    char stack[STACK_SIZE];
    char *top;
    Frame *frame;

    Stack (): top(stack), fp(0) {}

    // must be followed by pushFrame
    void *allocate (int size) {
        size = (size + 7) & ~7;
        char *p = top;
        top += size;
        return p;
    }

    void pushFrame (Frame *f) {
        f.up = frame;
        frame = f;
    }

    void popFrame () {
        if (!frame) {
            return;
        }

        Frame *f = frame;
        frame = f->up;

        if (reinterpret_cast<char*>(f) >= stack &&
                reinterpret_cast<char*>(f) < stack + STACK_SIZE) {
            f->~Frame();
            sp = reinterpret_cast<char*>(f);
        }
    }
};
#endif

