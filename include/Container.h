#ifndef CONTAINER_H
#define CONTAINER_H
#include <tbb/mutex.h>
//RAII Exception/thread safe container.
template<typename C>
class LockableContainer {
private:
    C m_Container;
    tbb::mutex Mutex;
    template<typename C> friend class LockableContainerLock;
};

template<typename C>
class LockableContainerLock {
private:
    LockableContainer<C>& c_;
public:
    LockableContainerLock(LockableContainer<C>& c) : c_(c) { c.Mutex.lock(); };
    ~LockableContainerLock(){ c_.Mutex.unlock(); };
	C* operator->() {return &c_.m_Container;}
};

#endif