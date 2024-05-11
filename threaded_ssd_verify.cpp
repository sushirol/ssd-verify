// Copyright (c) 2024 Arista Networks, Inc.  All rights reserved.
// Arista Networks, Inc. Confidential and Proprietary.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif


std::mutex mtx; // Mutex for synchronized output

void readBlock(const std::string& ssd_device, uint64_t block_number, 
               int block_size, int fd, char * buffer,
               std::ofstream& success_log, std::ofstream& failed_log) {
    off_t offset = block_number * block_size;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        std::cerr << "Error seeking to block offset" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mtx);
    ssize_t bytes_read = read(fd, buffer, block_size);
    if (bytes_read == -1) {
        std::cerr << "Error reading block: " << block_number << std::endl;
        failed_log << "Error reading block: " << block_number << std::endl;
    } else if (bytes_read != block_size) {
        std::cerr << "Incomplete block read: " << block_number << std::endl;
        failed_log << "Incomplete block read: " << block_number << std::endl;
    } else {
	success_log << "Block " << block_number << ": Read successful" << std::endl;
    }
    return;

}

long long get_block_device_size(const char* device_path, long long* total_bytes,
        unsigned long* block_size, int fd) {

    if (ioctl(fd, BLKGETSIZE64, total_bytes) == -1) {
        std::cerr << "Device " << device_path << "BLKGETSIZE64 ioctl failed" << std::endl;
	return 1;
    }
    if (ioctl(fd, BLKSSZGET, block_size) < 0) {
        std::cerr << "Device " << device_path << "BLKSSZGET ioctl failed" << std::endl;
        return 1;
    }

    std::cout << "Block device size in bytes: " << *total_bytes << std::endl;
    std::cout << "Device Block size in bytes: " << *block_size << std::endl;
    return 0;
}

int main() {
    std::string ssd_device = "/dev/sda";  // Specify the SSD device here
    unsigned long block_size = 0;         // Specify the block size of your SSD in bytes
    // total size of the device in bytes
    long long total_bytes = 0;
    // Container to hold thread objects
    std::vector<std::thread> threads;

    // Number of concurrent threads (adjust as needed)
    int num_threads = std::thread::hardware_concurrency();
    int* fd = new int[num_threads];
    for (int i = 0; i < num_threads; i++) {
        fd[i] = open(ssd_device.c_str(), O_RDONLY | O_DIRECT);
        if (fd[i] == -1) {
            std::cerr << "Error opening SSD device" << std::endl;
            return 1;
        }
    }

    int res = get_block_device_size(ssd_device.c_str(), &total_bytes,
            &block_size, fd[0]);
    if(res || block_size == 0 || total_bytes == 0) {
        std::cerr << "get_block_device_size failed" << std::endl;
        return 1;
    }
    // Read multiple blocks to speed up.
    block_size = block_size * 256;
    // Calculate the total number of blocks based on the block size and total size
    uint64_t total_blocks = total_bytes / block_size;
    std::cout << "Total number of blocks : " << total_blocks << std::endl;
    std::cout << "Total number of bytes : " << total_bytes << std::endl;
    std::cout << "Read block size : " << block_size << std::endl;

    std::ofstream success_log("/tmp/success");
    std::ofstream failed_log("/tmp/failed");

    char ** buffer = new char*[num_threads];;
    for (int i = 0; i < num_threads; i++) {
        posix_memalign((void**)&buffer[i], block_size, block_size);
    }
    // Launch threads to read blocks concurrently
    for (uint64_t block_number = 0; block_number < total_blocks;
            block_number += num_threads) {
        for (int i = 0;
             i < num_threads && (block_number + i) < total_blocks;
             i++)
        {
            threads.emplace_back(readBlock, ssd_device, block_number + i,
                                 block_size, fd[i], buffer[i],
                                 std::ref(success_log), std::ref(failed_log));
        }
        // Join threads to wait for them to finish
        for (auto& thread : threads) {
            thread.join();
        }
        // Clear the vector for the next batch of threads
        threads.clear();
    }

    for (int i = 0; i < num_threads; i++) {
        free(buffer[i]);
        close(fd[i]);
    }
    success_log.close();
    failed_log.close();

    return 0;
}

