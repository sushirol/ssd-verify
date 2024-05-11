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
#include <sys/statvfs.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif

std::mutex mtx; // Mutex for synchronized output

bool createFileToFillPartition(const std::string& filePath) {
    // Open a file at the specified path for writing
    std::ofstream outfile(filePath, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error: Unable to open file for writing" << std::endl;
        return false;
    }

    struct statvfs stat;
    if (statvfs("/mnt/flash", &stat) != 0) {
        std::cerr << "Error: Unable to get filesystem statistics" << std::endl;
        outfile.close();
        return false;
    }
    unsigned long long totalSize = stat.f_blocks * stat.f_frsize; // Total size in bytes
    unsigned long long ninetyPercent = (9 * totalSize) / 10; // 90% of total size
    unsigned long long freeSpace = stat.f_bfree * stat.f_frsize; // Free space in bytes
    std::cout << "totalSize: " << totalSize << " ninetyPercent: " << ninetyPercent << std::endl;
    unsigned long long bytesWritten = totalSize - freeSpace;
    std::cout << "freeSpace: " << freeSpace << " Initial bytesWritten: " << bytesWritten << std::endl;
    const char data[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
    while (bytesWritten < ninetyPercent) {
        unsigned long long remainingBytes = ninetyPercent - bytesWritten;
        unsigned long long writeSize = sizeof(data) - 1; // Exclude null terminator
        if (writeSize > remainingBytes) {
            writeSize = remainingBytes;
        }
        outfile.write(data, writeSize);
        if (!outfile) {
            std::cerr << "Error: Write operation failed" << std::endl;
            outfile.close();
            return false;
        }
        bytesWritten += writeSize;
        /*
         *std::cout << "bytesWritten: " << bytesWritten<< std::endl;
         */
    }

    // Close the file
    outfile.close();

    std::cout << "File created and filled up to 90% of partition size successfully" << std::endl;
    return true;
}

int main() {
    std::string filePath = "/mnt/flash/FillData";
    if (createFileToFillPartition(filePath)) {
        std::cout << "SSD filled at least 90%" << std::endl;
    } else {
        std::cerr << "Could not fill SSD" << std::endl;
    }
    return 0;
}

