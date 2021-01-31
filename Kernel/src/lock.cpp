#include <lock.h>

#include <scheduler.h>
#include <timer.h>
#include <cpu.h>
#include <logging.h>

bool Semaphore::Wait(){
    acquireLock(&lock);
    __sync_fetch_and_sub(&value, 1);

    if(value < 0){
        SemaphoreBlocker blocker(this);
        blocked.add_back(&blocker);

        releaseLock(&lock);

        return Scheduler::GetCurrentThread()->Block(&blocker);
    }
    releaseLock(&lock);

    return false;
}

bool Semaphore::WaitTimeout(long& timeout){
    acquireLock(&lock);
    __sync_fetch_and_sub(&value, 1);

    if(value < 0){
        SemaphoreBlocker blocker(this);
        blocked.add_back(&blocker);

        releaseLock(&lock);

        return Scheduler::GetCurrentThread()->Block(&blocker, timeout);
    }
    releaseLock(&lock);

    return false;
}

void Semaphore::Signal(){
    acquireLock(&lock);

    __sync_fetch_and_add(&value, 1);
    if(blocked.get_length() > 0){
        SemaphoreBlocker* blocker = blocked.remove_at(0);
        blocker->Unblock();
    }

    releaseLock(&lock);
}