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


uint16_t pixels[IMAGE_HEIGHT][IMAGE_WIDTH];
uint8_t  pixels_bw[IMAGE_HEIGHT][IMAGE_WIDTH];
uint8_t pixels_scaled[28][28];


int main(void) {
    volatile unsigned int *video_in_dma = NULL;
    volatile unsigned int *KEY_ptr = NULL;
    volatile unsigned short *video_mem = NULL;
    void *virtual_base, *video_base;
    int fd;

    // Open /dev/mem
    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    // Map control registers
    virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(fd);
        return 1;
    }
    KEY_ptr = (unsigned int *)(virtual_base + (PUSH_BASE & HW_REGS_MASK));
    video_in_dma = (unsigned int *)(virtual_base + (VIDEO_BASE & HW_REGS_MASK));

    // Start video streaming
    *(video_in_dma + 3) = 0x4;

    while (1) {
        if (!(*KEY_ptr & 0x1)|!(*KEY_ptr & 0x2)|!(*KEY_ptr & 0x4)) {
            printf("photo captured\n");
            while (!(*KEY_ptr & 0x1)|!(*KEY_ptr & 0x2)|!(*KEY_ptr & 0x4)) {
            }
            *(video_in_dma + 3) = 0x0;
            break;
        }

        usleep(10000);
    }

    // Map video memory
    video_base = mmap(NULL, IMAGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, FPGA_ONCHIP_BASE);
    if (video_base == MAP_FAILED) {
        printf("ERROR: mmap() video memory failed...\n");
        close(fd);
        return 1;
    }

    video_mem = (volatile unsigned short *)video_base;

    // Fill pixel buffers
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            unsigned short pixel = *(video_mem + (y << 9) + x);

            pixels[y][x] = pixel;

            // Extract RGB565 and convert to grayscale
            int red   = (pixel >> 11) & 0x1F;
            int green = (pixel >> 5)  & 0x3F;
            int blue  = pixel & 0x1F;
            int gray = (red * 30 + green * 59 + blue * 11) / 100;

            pixels_bw[y][x] = gray & 0xFF;
        }
    }
    scaleImagePreservingAspectRatio(&pixels_bw[0][0], &pixels_scaled[0][0], 240, 240, 28, 28);
    saveImageGrayscale("first_image_mnist.bmp", &pixels_scaled[0][0], 28, 28);

    // Save BMP files
    //saveImageShort("color.bmp", &pixels[0][0], IMAGE_WIDTH, IMAGE_HEIGHT);
    // saveImageGrayscale("bw.bmp", &pixels_bw[0][0], IMAGE_WIDTH, IMAGE_HEIGHT);

    // Cleanup
    if (munmap(video_base, IMAGE_SPAN) != 0 || munmap(virtual_base, HW_REGS_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

