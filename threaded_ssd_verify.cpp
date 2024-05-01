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
    /*
     *std::string dd_command = "dd if=" + ssd_device + " of=/dev/null skip=" + std::to_string(block_number)
     *                          + " bs=" + std::to_string(block_size) + " count=1 status=none iflag=direct";
     *int result = system(dd_command.c_str());
     */
    std::ifstream ssd_input(ssd_device, std::ios::binary);
    if (!ssd_input) {
        failed_log <<"Error opening SSD device" << std::endl;
        std::cerr << "Error opening SSD device" << std::endl;
    }
    // Seek to the desired block offset
    ssd_input.seekg(block_number * block_size);
    char buffer[block_size];
    ssd_input.read(buffer, block_size);

    std::lock_guard<std::mutex> lock(mtx);
    if (ssd_input.gcount() != block_size) {
        std::cerr << "Block " << block_number << ": Read failed" << std::endl;
        failed_log << "Block " << block_number << ": Read failed" << std::endl;
    /*
     *} else {
     *    success_log << "Block " << block_number << ": Read successful" << std::endl;
     */
    }
}

long long get_block_device_size(const char* device_path) {
    struct stat statbuf;
    long long total_bytes;

    // Try stat first (simpler approach)
    if (stat(device_path, &statbuf) != -1 && statbuf.st_blocks != 0) {
	if (S_ISBLK(statbuf.st_mode)) {
	    total_bytes = statbuf.st_blksize * statbuf.st_blocks;
	    std::cout << "statbuf Total number of bytes : " << statbuf.st_blksize << " * " <<  statbuf.st_blocks << std::endl;
	    return total_bytes;
	} else {
	    std::cerr << device_path << " is not a block device." << std::endl;
	    return -1;
	}
    }

    // Fallback to ioctl if stat fails
    int fd = open(device_path, O_RDONLY);
    if (fd == -1) {
	perror("open");
	return -1;
    }

    if (ioctl(fd, BLKGETSIZE64, &total_bytes) == -1) {
	perror("ioctl");
	close(fd);
	return -1;
    }

    close(fd);
    std::cout << "IOCTL: Total number of bytes : " << total_bytes << std::endl;
    return total_bytes;
}

int main() {
    std::string ssd_device = "/dev/sda";  // Specify the SSD device here
    int block_size = 512;                 // Specify the block size of your SSD in bytes

    // Get the total size of the device in bytes
    long long total_bytes = get_block_device_size(ssd_device.c_str());

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

