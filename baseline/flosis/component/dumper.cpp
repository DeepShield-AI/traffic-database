#include "dumper.hpp"

void Dumper::bindCore(u_int32_t cpu){
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
        printf("Dumper log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Dumper warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

void Dumper::dump(char* buffer, u_int64_t size){
    ssize_t ret = pwrite(this->disk_fd, (char*)buffer, size, this->offset + this->write_place);
    if (ret < 0) {
        printf("Dumper error: write on %lu fail!\n",size);
        throw std::runtime_error(std::string("write failed: ") + strerror(errno));
    }
    this->write_place += size;
    this->write_place %= this->capacity;
}

int Dumper::run(){
    if(this->bind_core){
        this->bindCore(this->core_id);
    }
    this->stop = false;
    while(true){
        if(this->stop){
            break;
        }
        char* buffer = this->wbuffer->get();
        printf("Dumper log: dump on %lu\n",this->write_place);
        this->dump(buffer, this->wbuffer->getBlockSize());
    }
    return 0;
}
void Dumper::asynchronousStop(){
    this->stop = true;
}