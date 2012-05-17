#ifndef STACK_H
#define STACK_H

#include "Log.h"

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

    enum STATE { COMPLETE, CONTINUE, STOPPED, ABORTED };

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

    Frame::STATE run () {
        //Log l("stackrunner ");
        while (frame) {
            //l.debug("running %08x...\n", frame);
            Frame::STATE s = frame->run(this);
            //l.debug("returned %d\n", s);
            switch (s) {
            case Frame::STOPPED:
                return Frame::STOPPED;
            case Frame::CONTINUE:
                break;
            case Frame::COMPLETE:
                pop();
                break;
            case Frame::ABORTED:
                while (frame) pop();
                return Frame::ABORTED;
            default:
                // should never occur
                ;
            }
        }
        return Frame::COMPLETE;
    }
};

}
#endif

