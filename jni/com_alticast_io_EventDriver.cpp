#include <assert.h>
#include "EventDriver.h"
#include "ThreadLocal.h"
#include "com_alticast_io_EventDriver.h"
#include "al_debug.h"

using namespace std;
using namespace acba;

THREAD_LOCAL(JNIEnv*, jniEnv);
static jmethodID midProcessEvent;

struct NativeReactor: Reactor {
    jobject hReactor;
    NativeReactor (bool s): Reactor(s), hReactor(0) {}
    void processEvent (int events) {

        assert(hReactor);
        assert(THREAD_GET(jniEnv));
        assert(midProcessEvent);

        THREAD_GET(jniEnv)->CallVoidMethod(hReactor, midProcessEvent, events,
                getEventDriver() != 0);
    }
};

static void refReactor (JNIEnv *env, NativeReactor *r, jobject hReactor)
{
    if (!r->hReactor) {
        r->hReactor = env->NewGlobalRef(hReactor);
    }
}

static jboolean tryUnrefReactor (JNIEnv *env, NativeReactor *r)
{
    EventDriver *ed = r->getEventDriver();
    if (ed) return JNI_FALSE; // still bound.
    if (!r->hReactor) return JNI_FALSE; // already unrefed.

    env->DeleteGlobalRef(r->hReactor);
    r->hReactor = 0;
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_com_alticast_io_EventDriver_nCreateEventDriver (
        JNIEnv *env, jclass clz)
{
    if (!midProcessEvent) {
        midProcessEvent = env->GetMethodID(env->FindClass(
                "com/alticast/io/Reactor"), "guardedProcessEvent", "(I)V");
    }
    EventDriver *ed = new EventDriver();
    return reinterpret_cast<jlong>(ed);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nDeleteEventDriver (
        JNIEnv *env, jclass clz, jlong ned)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    delete ed;
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nWatchFile (JNIEnv *env,
        jclass clz, jlong ned, jint fd, jint fmask, jlong nr, jobject hReactor)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    refReactor(env, r, hReactor);
    ed->watchFile(fd, fmask, r);
}

JNIEXPORT jboolean JNICALL Java_com_alticast_io_EventDriver_nUnwatchFile (
        JNIEnv *env, jclass clz, jlong ned, jlong nr)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    ed->unwatchFile(r);
    return tryUnrefReactor(env, r);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nSetTimeout (
        JNIEnv *env, jclass clz, jlong ned, jlong usec, jlong nr,
        jobject hReactor)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    refReactor(env, r, hReactor);
    ed->setTimeout(usec, r);
}

JNIEXPORT jboolean JNICALL Java_com_alticast_io_EventDriver_nUnsetTimeout (
        JNIEnv *env, jclass clz, jlong ned, jlong nr)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    ed->unsetTimeout(r);
    return tryUnrefReactor(env, r);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nUnbind (
        JNIEnv *env, jclass clz, jlong ned, jlong nr)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    ed->unbind(r);
    tryUnrefReactor(env, r);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nWait (
        JNIEnv *env, jclass clz, jlong ned, jlong usec)
{
    if (!THREAD_GET(jniEnv)) {
        THREAD_GET(jniEnv) = env;
    }
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    ed->wait(usec);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nRun (
        JNIEnv *env, jclass clz, jlong ned)
{
    if (!THREAD_GET(jniEnv)) {
        THREAD_GET(jniEnv) = env;
    }
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    ed->run();
}

JNIEXPORT jboolean JNICALL Java_com_alticast_io_EventDriver_nIsEventThread (
        JNIEnv *env, jclass clz, jlong ned)
{
    EventDriver *ed = reinterpret_cast<EventDriver*>(ned);
    return ed->isEventThread();
}

JNIEXPORT jlong JNICALL Java_com_alticast_io_EventDriver_nCreateReactor (
        JNIEnv *env, jclass clz, jboolean s)
{
    NativeReactor *r = new NativeReactor(s);
    return reinterpret_cast<jlong>(r);
}

JNIEXPORT void JNICALL Java_com_alticast_io_EventDriver_nDeleteReactor (
        JNIEnv *env, jclass clz, jlong nr)
{
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    assert(!r->hReactor || !"Native Ref exists after gc");
    assert(!r->getEventDriver() || !"Reactor is still bound");
    delete r;
}

JNIEXPORT jboolean JNICALL Java_com_alticast_io_EventDriver_nTryUnrefReactor (
        JNIEnv *env, jclass clz, jlong nr)
{
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    return tryUnrefReactor(env, r);
}

JNIEXPORT jint JNICALL Java_com_alticast_io_EventDriver_nGetMask (JNIEnv *env,
    jclass clz, jlong nr)
{
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    return r->getMask();
}

JNIEXPORT jint JNICALL Java_com_alticast_io_EventDriver_nGetFd (JNIEnv *env,
    jclass clz, jlong nr)
{
    NativeReactor *r = reinterpret_cast<NativeReactor*>(nr);
    return r->getFd();
}
