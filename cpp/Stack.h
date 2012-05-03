#ifndef STACK_H
#define STACK_H

namespace base {

struct Stack;

/**
 * This represents a stack frame in a stack.
 * A frame contains local variables and a function to operate on.
 * Each run returns the resulting state of the frame.
 * COMPLETE means that this frame just finished its job, and its up-frame
 * should resume the execution. CONTINUE means this frame can be continued
 * after running sub-frame. STOPPED means this frame work blocked by
 * external reasons. ABORT means that unrecoverable exception happend, and
 * all frames will be rolled-back.
 */
struct Frame {

    enum STATE { STOPPED, CONTINUE, COMPLETE, ABORTED };

    Frame *up;
    int step;

    Frame (): step(0) {}

    virtual STATE run (Stack *stack) = 0;
    virtual ~Frame() {};
};

/**
 * Emulation of native stack.
 * This maintains stack structure of frames which could be 
 * allocated in the stack buffer or placed elsewhere.
 * If the frame was allocated in the stack buffer and finished its job,
 * it will be destroyed on pop.
 */
struct Stack {

    static const int STACK_SIZE = 1024;

    void *env;
    char stack[STACK_SIZE];
    char *top;
    Frame *frame;

    Stack (): top(stack), frame(0) {}

    // must be used by Frame construction, and followed by push
    void *allocate (int size) {
        size = (size + 7) & ~7;
        char *p = top;
        top += size;
        return p;
    }

    void push (Frame *f) {
        f->up = frame;
        frame = f;
    }

    void pop () {
        if (!frame) {
            return;
        }

        Frame *f = frame;
        frame = f->up;

        if (reinterpret_cast<char*>(f) >= stack &&
                reinterpret_cast<char*>(f) < stack + STACK_SIZE) {
            f->~Frame();
        }
    }
};

}
#endif

