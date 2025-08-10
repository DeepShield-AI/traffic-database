#include "indexPersister.hpp"

u_int64_t IndexPersister::bit_ceil(u_int32_t x){
    if (x <= 1) return 1;
    --x;
    for (size_t i = 1; i < sizeof(u_int32_t) * 8; i <<= 1) {
        x |= x >> i;
    }
    return ++x;
}

void IndexPersister::bindCore(u_int32_t cpu){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    pthread_t thread = pthread_self();

    int set_result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_result != 0) {
        std::cerr << "Error setting thread affinity: " << set_result << std::endl;
    }

    // 确认设置是否成功
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    if (CPU_ISSET(cpu, &cpuset)) {
        printf("Index Persister log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Index Persister warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

void IndexPersister::setThreadID(u_int64_t threadID){
    this->thread_id = threadID;
}

int IndexPersister::run(){
    if(this->thread_id == std::numeric_limits<uint64_t>::max()){
        printf("Index Persister error: run without thread id!\n");
        return -1;
    }

    if(this->bind_core){
        // this->bindCore(this->rx_id*2 + 72);
        this->bindCore(this->core_id);
    }

    std::cout << "Index Persister log: thread run." << std::endl;
    this->stop = false;

    while(true){
        if(this->stop){
            break;
        }
        u_int64_t checkID = *(this->skiplistCheckID)++;
        checkID %= this->skiplist_check_roll;
        while(checkID >= IndexType::TOTAL){
            checkID = *(this->skiplistCheckID)++;
            checkID %= this->skiplist_check_roll;
            break;
        }
        
        auto skiplist = (*(this->skiplists))[checkID];
        if(skiplist == nullptr){
            continue;
        }

        while(skiplist->)
    }
}

void IndexPersister::asynchronousStop(){
    this->stop = true;
}