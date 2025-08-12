#ifndef BITMAP_HPP_
#define BITMAP_HPP_
#include <iostream>
#include <sys/mman.h>
#include <algorithm>
#include <climits>
#include <atomic>

// 使用位图来表示索引

class BitMap{
private:
    u_int8_t* bitmap;
    u_int64_t row_count; // equal to Bloom filter num
    u_int64_t col_count;
    // u_int64_t real_col_count; // equal to block_num + t (t is backup for clearing)
    // u_int64_t logic_col_count; // equal to block_num
    u_int64_t backup_col_count; 
    u_int64_t size;

    // std::atomic_uint64_t begin_col; // begin_col
    // std::atomic_uint64_t cleaning_col; // the column being cleaned, used for multi-threading

    // std::atomic_bool* col_dirty;
    // std::atomic_bool* col_cleaning;

    u_int64_t getByteIndex(u_int64_t row, u_int64_t col) const{
        if (row >= this->row_count || col >= this->col_count){
            printf("Bitmap error: row %llu and col %llu out of range!\n",row,col);
            return std::numeric_limits<u_int64_t>::max();
        }
        u_int64_t byte_col = col % (this->col_count/sizeof(u_int8_t));
        return row * this->col_count + byte_col;
    }
    u_int64_t getBitIndex(u_int64_t col) const{
        if (col >= this->col_count){
            printf("Bitmap error: col %llu out of range!\n", col);
            return std::numeric_limits<u_int64_t>::max();
        }
        return col / (this->col_count/sizeof(u_int8_t));
    }
    // u_int64_t getRealColRead(u_int64_t logic_col) const{
    //     u_int64_t real_col = logic_col + this->begin_col.load();
    //     u_int64_t tmp_col = (real_col + this->backup_col_count) % this->real_col_count;
    //     u_int64_t left_barrier = this->cleaning_col.load();
    //     if (tmp_col >= left_barrier){
    //         if (tmp_col < left_barrier + this->backup_col_count){ // Retrieve invalid area
    //             return std::numeric_limits<u_int64_t>::max();
    //         }
    //         return tmp_col;
    //     }
    //     return real_col % this->real_col_count;
    // }
    // u_int64_t getRealColWrite(u_int64_t logic_col) const{
    //     u_int64_t real_col = logic_col + this->begin_col.load();
    //     u_int64_t right_barrier = this->cleaning_col.load();
    //     u_int64_t tmp_col = (real_col + this->backup_col_count) % this->real_col_count;
    //     if (tmp_col > right_barrier){
    //         real_col = tmp_col;
    //     }
    //     real_col %= this->real_col_count;

    //     u_int64_t left_barrier = (right_barrier + this->real_col_count - 2 * this->backup_col_count) % this->real_col_count;
    //     if (left_barrier < right_barrier && real_col >= left_barrier && real_col < right_barrier){
    //         return real_col;
    //     }
    //     if (left_barrier >= right_barrier && (real_col >= left_barrier || real_col < right_barrier)){
    //         return real_col;
    //     }
    //     // new block
    //     real_col = (real_col + this->logic_col_count) % this->real_col_count;
    //     if (left_barrier < right_barrier && real_col >= left_barrier && real_col < right_barrier){
    //         return real_col;
    //     }
    //     if (left_barrier >= right_barrier && (real_col >= left_barrier || real_col < right_barrier)){
    //         return real_col;
    //     }
    //     return std::numeric_limits<u_int64_t>::max();
    // }
public:
    // backup_col_count should be bigger than RSS_NUM*4
    BitMap(u_int64_t row_count, u_int64_t logic_col_count, u_int64_t backup_col_count): 
        row_count(row_count), col_count(logic_col_count + backup_col_count), backup_col_count(backup_col_count) {
        if (this->col_count % sizeof(u_int8_t)){
            printf("Bitmap error: col_count %llu is not a multiple of u_int8_t size!\n", col_count);
            throw std::runtime_error("Invalid column count for bitmap");
        }
        if (this->backup_col_count * 8 > this->col_count){
            printf("Bitmap error: logic_col_count %llu is not bigger than 8 times of backup_col_count %llu!\n", col_count, backup_col_count);
            throw std::runtime_error("Invalid column count for bitmap");
        }
        this->size = this->row_count * this->col_count / 8;
        this->bitmap = (u_int8_t*)mmap(nullptr, this->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->bitmap == MAP_FAILED){
            printf("Disk buffer error: mmap failed for disk metas!\n");
            throw std::runtime_error("Disk buffer mmap failed");
        }
        for (u_int64_t i = 0; i < this->size; ++i) {
            this->bitmap[i] = 0; // Initialize the bitmap to zero
        }
        // this->begin_col = 0;
        // this->cleaning_col = this->backup_col_count;
        // bitmap = new u_int8_t[size]();
    }
    ~BitMap() {
        munmap((void*)this->bitmap, this->size);
    }

    u_int64_t getRowCount() const {
        return this->row_count;
    }
    u_int64_t getColCount() const {
        return this->col_count;
    }
    u_int64_t getBackupColCount() const {
        return this->backup_col_count;
    }

    bool get(u_int64_t row, u_int64_t col) const{
        // if (col >= this->col_count){
        //     printf("Bitmap error: col %llu out of range!\n", col);
        //     return false;
        // }
        // u_int64_t real_col = this->getRealColRead(logic_col);
        // if(real_col == std::numeric_limits<u_int64_t>::max()){
        //     return false;
        // }
        u_int64_t byte_index = this->getByteIndex(row, col);
        if (byte_index == std::numeric_limits<u_int64_t>::max()) return false;
        u_int64_t bit_index = this->getBitIndex(col);
        bool ret = (bitmap[byte_index] & (1 << bit_index)) != 0;
        // if (col == this->cleaning_col.load()) return false;
        return ret;
    }
    // only used by one cleaning thread
    // void barrierMove(){
    //     u_int64_t col = this->cleaning_col.load();
    //     col = (col + 1) % this->real_col_count;
    //     this->cleaning_col.store(col);
    //     u_int64_t begin = this->begin_col.load();
    //     if (col == begin){
    //         begin = (begin + this->logic_col_count) % this->real_col_count;
    //         this->begin_col.store(begin);
    //     }
    // }
    void clearCol(u_int64_t col) {
        // if (row_end > this->row_count){
        //     printf("Bitmap error: row end %llu out of range!\n", row_end);
        // }
        // u_int64_t col = this->cleaning_col.load();
        u_int64_t bit_index = getBitIndex(col);
        if (bit_index == std::numeric_limits<u_int64_t>::max()) return;
        for (u_int64_t row = 0; row < this->row_count; ++row){
            u_int64_t byte_index = getByteIndex(row, col);           
            bitmap[byte_index] &= ~(1 << bit_index);
        }
    }
    // set col should in the write field (cleaning - 2*backup, cleanning)
    void set(u_int64_t row, u_int64_t col) {
        // if (logic_col >= this->logic_col_count){
        //     printf("Bitmap error: logic_col %llu out of range!\n", logic_col);
        //     return;
        // }
        // u_int64_t real_col = this->getRealColWrite(logic_col);
        // if (real_col == std::numeric_limits<u_int64_t>::max()){
        //     printf("Bitmap warning: logic_col %llu out of cleaning_col.\n", logic_col);
        // }
        u_int64_t byte_index = this->getByteIndex(row, col);
        if (byte_index == std::numeric_limits<u_int64_t>::max()) return;
        u_int64_t bit_index = this->getBitIndex(col);
        bitmap[byte_index] |= (1 << bit_index);
    }
};

#endif