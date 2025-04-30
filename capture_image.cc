#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include "bmp_utility.h"

#define HW_REGS_BASE (0xff200000)
#define HW_REGS_SPAN (0x00200000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)
#define LED_BASE 0x1000
#define PUSH_BASE 0x3010
#define VIDEO_BASE 0x0000

#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 240


#define FPGA_ONCHIP_BASE     (0xC8000000)
#define IMAGE_SPAN (IMAGE_WIDTH * IMAGE_HEIGHT * 4)
#define IMAGE_MASK (IMAGE_SPAN - 1)


int main(void) {
    volatile unsigned int *video_in_dma = NULL;
    volatile unsigned int *key_ptr = NULL;
    volatile unsigned short *video_mem_ptr = NULL;
    void *video_mem;
    void *virtual_base;
    int fd;

    // Open /dev/mem
    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    // Map physical memory into virtual address space
    virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(fd);
        return 1;
    }

    video_mem = mmap(NULL, IMAGE_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FPGA_ONCHIP_BASE);
    if(video_mem == MAP_FAILED){
        printf("video memory failed\n");
        munmap(virtual_base, HW_REGS_SPAN);
        close(fd);
        return 1;
    }


    // Calculate the virtual address where our device is mapped
    video_in_dma = (volatile unsigned int *)((char*)virtual_base + ((VIDEO_BASE) & (HW_REGS_MASK)));
    key_ptr = (volatile unsigned int *)((char*)virtual_base + (PUSH_BASE & HW_REGS_MASK));
    video_mem_ptr = (unsigned short *)(video_mem + (FPGA_ONCHIP_BASE & IMAGE_MASK));


    printf("Video In DMA register updated at:0%x\n",(video_in_dma));

    // Modify the PIO register
    *(video_in_dma+3) = 0x4;
    //*h2p_lw_led_addr = *h2p_lw_led_addr + 1;

    while ((*key_ptr & 0x7) == 0x7){
        usleep(10000);
    }
    *(video_in_dma+3) = 0x0;
    usleep(10000);


    unsigned char pixels_bw[IMAGE_HEIGHT][IMAGE_WIDTH];
    for (int y = 0; y < IMAGE_HEIGHT; y++){
        for (int x = 0; x < IMAGE_WIDTH; x++){
                pixels_bw[y][x] = *(video_mem_ptr + (y << 9) +x);
        }
    }

    unsigned char pixels_scaled[28][28];
    scaleImagePreservingAspectRatio(&pixels_bw[0][0],&pixels_scaled[0][0],240,240,28,28);
    const char* filename1 = "final_image_bw6.bmp";
    //saving image as black and white
    saveImageGrayscale(filename1,(const unsigned char*)&pixels_scaled[0][0],28,28);


    // Clean up
    if (munmap(virtual_base, IMAGE_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
