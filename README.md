 # Maps:

 ## IntrusiveMap<K,V>
 * Key (K) - any with nothrow ==, !=, <
 * Value (T) - any, with public fields m_left, m_right, m_parent, m_key
 * No useless Node allocation
   * Nothrow exception guarantee WO transactional semantics
   * No alloc call - usefull for using with locks

 ## Map<K, V, Lock>
 * Key (K) - any with nothrow ==, !=, <
 * Value (T) - any
 * Lock - BasicLockable. Default - empty lock.
 * Facade for IntrusiveMap<K,T>.

 # Queues: 

 ## IntrusiveMutexFreeQueue
  * Value (T) - any, with public field m_next (type: T*)
  * Push WO locks
  * No sleeps
  * No allocations

 ## MutexFreeQueue
  * Value (T) - any
  * Push WO locks
  * No sleeps

 # Stacks:

 ## IntrusiveMutexFreeStack
  * Value (T) - any, with public field m_next (type: T*)
  * No sleeps
  * No allocations

 ## MutexFreeStack
  * Value (T) - any
  * No sleeps
