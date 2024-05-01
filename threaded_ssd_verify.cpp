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

void readBlock(const std::string& ssd_device, uint64_t block_number, int block_size,
               std::ofstream& success_log, std::ofstream& failed_log) {
    char * buffer = new char [block_size];
    std::ifstream ssd_input(ssd_device, std::ios::binary);
    if (!ssd_input) {
        failed_log <<"Error opening SSD device" << std::endl;
        std::cerr << "Error opening SSD device" << std::endl;
    }
    // Seek to the desired block offset
    ssd_input.seekg(block_number * block_size);
    ssd_input.read(buffer, block_size);

    if (!ssd_input || (ssd_input.gcount() != block_size)) {
        std::lock_guard<std::mutex> lock(mtx);
        std::cerr << "Block " << block_number << ": Read failed" << std::endl;
        failed_log << "Block " << block_number << ": Read failed" << std::endl;
    } else {
        success_log << "Block " << block_number << ": Read successful" << std::endl;
    }
    delete[] buffer;
    ssd_input.close();
}

long long get_block_device_size(const char* device_path, long long* total_bytes,
        unsigned long* block_size) {

    int fd = open(device_path, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Device " << device_path << "open failed" << std::endl;
	return 1;
    }
    if (ioctl(fd, BLKGETSIZE64, total_bytes) == -1) {
        std::cerr << "Device " << device_path << "BLKGETSIZE64 ioctl failed" << std::endl;
	close(fd);
	return 1;
    }
    if (ioctl(fd, BLKSSZGET, block_size) < 0) {
        std::cerr << "Device " << device_path << "BLKSSZGET ioctl failed" << std::endl;
        return 1;
    }

    close(fd);
    std::cout << "Block device size in bytes: " << *total_bytes << std::endl;
    std::cout << "Block size in bytes: " << *block_size << std::endl;
    return 0;
}

int main() {
    std::string ssd_device = "/dev/sda";  // Specify the SSD device here
    unsigned long block_size = 0;                 // Specify the block size of your SSD in bytes

    // Get the total size of the device in bytes
    long long total_bytes = 0;
    int res = get_block_device_size(ssd_device.c_str(), &total_bytes, &block_size);
    if(res || block_size == 0 || total_bytes == 0) {
        std::cerr << "get_block_device_size failed" << std::endl;
        return 1;
    }

    block_size = block_size * 16; //speed up reading blocks
    // Calculate the total number of blocks based on the block size and total size
    uint64_t total_blocks = total_bytes / block_size;
    std::cout << "Total number of blocks : " << total_blocks << std::endl;
    std::cout << "Total number of bytes : " << total_bytes << std::endl;
    std::cout << "Block size : " << block_size << std::endl;

    std::ofstream success_log("/tmp/success");
    std::ofstream failed_log("/tmp/failed");

    // Container to hold thread objects
    std::vector<std::thread> threads;

    // Number of concurrent threads (adjust as needed)
    int num_threads = std::thread::hardware_concurrency();
    std::cout << "number of threads : " << num_threads << std::endl;

    // Launch threads to read blocks concurrently
    for (uint64_t block_number = 0; block_number < total_blocks; block_number += num_threads) {
        for (int i = 0; i < num_threads && (block_number + i) < total_blocks; i++) {
            threads.emplace_back(readBlock, ssd_device, block_number + i, block_size,
                                 std::ref(success_log), std::ref(failed_log));
        }

        // Join threads to wait for them to finish
        for (auto& thread : threads) {
            thread.join();
        }

        threads.clear(); // Clear the vector for the next batch of threads
    }

    success_log.close();
    failed_log.close();

    return 0;
}

