#ifndef CONTAINER_H
#define CONTAINER_H
//RAII Exception/thread safe container.
template<typename C>
class LockableContainer {
private:
    C m_Container;
    std::mutex Mutex;
    template<typename C> friend class LockableContainerLock;
};

template<typename C>
class LockableContainerLock {
private:
    LockableContainer<C>& c_;
	int destroyed;
public:
    LockableContainerLock(LockableContainer<C>& c) : c_(c) { c.Mutex.lock(); destroyed = 0;};
    ~LockableContainerLock(){ if(!destroyed) c_.Mutex.unlock(); };
	//WARNING: This function breaks the lock (essentially the same as destroying this class instance!)
	void unlock() { c_.Mutex.unlock(); destroyed = 1;} 
	C* operator->() {return &c_.m_Container;}
};

#endif