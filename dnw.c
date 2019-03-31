//
// Copyright (C) 2019 Shellever <shellever@163.com>
// License: GPL v2
//

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <usb.h>
#include <getopt.h>

// JZ2440V2
// USB:    IN_ENDPOINT:1 OUT_ENDPOINT:3
// FORMAT: <ADDR(DATA):4>+<SIZE(n+10):4>+<DATA:n>+<CS:2>
#define JZ2440_RAM_BASE     0x30000000
#define JZ2440_VENDOR_ID    0x5345
#define JZ2440_PRODUCT_ID   0x1234
#define JZ2440_EP_ADDR_OUT  0x03


static unsigned int load_addr = JZ2440_RAM_BASE;
static uint16_t id_vendor = JZ2440_VENDOR_ID;
static uint16_t id_product = JZ2440_PRODUCT_ID;
static uint8_t ep_addr_out = JZ2440_EP_ADDR_OUT;
static const char *bin_file = "u-boot.bin";


struct usb_dev_handle *open_usb_port(void) 
{
    struct usb_bus *busses, *bus;
    struct usb_device *dev;
    struct usb_dev_handle *hdev;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    printf("finding usb device\n");
    busses = usb_get_busses();
    for (bus = busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (id_vendor == dev->descriptor.idVendor 
                    && id_product == dev->descriptor.idProduct) {
                printf("found: idVendor=0x%x, idProduct=0x%x\n", 
                        id_vendor, id_product);
                if ((hdev = usb_open(dev)) == NULL) {
                    printf("cannot open usb device\n");
                    return NULL;
                }

                if (usb_claim_interface(hdev, 0) != 0) {
                    printf("cannot claim interface\n");
                    usb_close(hdev);
                    return NULL;
                }

                return hdev;
            }
        }
    }
    printf("target usb device not found!\n");

    return NULL;
}

unsigned char *prepare_write_buf(const char *filename, unsigned int *len, unsigned long load_addr) 
{
    unsigned char *write_buf = NULL;
    struct stat fs;
    int fd;
    uint16_t checksum = 0;
    unsigned char *cs_start, *cs_end;

    if ((fd = open(filename, O_RDONLY)) == -1) {
        printf("cannot open file %s\n", filename);
        return NULL;
    }

    if (fstat(fd, &fs) == -1) {
        printf("cannot get file %s\n size", filename);
        goto error;
    }

    write_buf = (unsigned char*) malloc(fs.st_size + 10);
    if (write_buf == NULL) {
        perror("malloc failed");
        goto error;
    }

    if (fs.st_size != read(fd, write_buf + 8, fs.st_size)) {
        perror("read file failed");
        goto error;
    }

    printf("filename: %s\n", filename);
    printf("filesize: %lu bytes (%lu kB)\n", fs.st_size, fs.st_size/1024);

    *((uint32_t *) write_buf) = load_addr;              // download address
    *((uint32_t *) write_buf + 1) = fs.st_size + 10;    // download size
    *len = fs.st_size + 10;

    // usb format: addr(4) + size(4) + data(n) + checksum(2)
    // calculate checksum value
    cs_start = write_buf + 8;
    cs_end = write_buf + 8 + fs.st_size;
    while (cs_start < cs_end) {
        checksum += *cs_start++;
    }
    printf("checksum: 0x%04X\n", checksum);
    *((uint16_t *)(write_buf + fs.st_size + 8)) = checksum;

    return write_buf;

error: 
    if (fd != -1) {
        close(fd);
    }
    if (write_buf != NULL) {
        free(write_buf);
    }
    fs.st_size = 0;

    return NULL;
}

void print_usage(const char *prog)
{
    printf("dnw - download file to device using usb\n");
    printf("Usage: %s [-aefhpv]\n", prog);
    puts("  -a, --address    address to download (default 0x30000000)\n"
         "  -e, --endpoint   endpoint address for output (default 0x03)\n"
         "  -f, --file       u-boot binary file (default u-boot.bin)\n"
         "  -h, --help       help information\n"
         "  -p, --product    product id (default 0x1234)\n"
         "  -v, --vendor     vendor id (default 0x5345)\n");
    printf("Example:\n");
    printf("  $ ./dnw\n");
    printf("  $ ./dnw -a 0x30000000 -f u-boot.bin\n\n");
    exit(1);
}

void parse_opts(int argc, char *argv[])
{
    int c;

    static const struct option lopts[] = {
        { "address",  1, 0, 'a' },
        { "endpoint", 1, 0, 'e' },
        { "file",     1, 0, 'f' },
        { "help",     0, 0, 'h' },
        { "product",  1, 0, 'p' },
        { "vendor",   1, 0, 'v' },
        { NULL, 0, 0, 0 },
    };

    while (1) {
        c = getopt_long(argc, argv, "a:e:f:hp:v:", lopts, NULL);
        if (c == -1) {
            break;
        }
 
        switch (c) {
        case 'a':
            load_addr = strtol(optarg, NULL, 16);
            break;
        case 'e':
            ep_addr_out = strtol(optarg, NULL, 16);
            break;
        case 'f':
            bin_file = optarg;
            break;
        case 'p':
            id_product = strtol(optarg, NULL, 16);
            break;
        case 'v':
            id_vendor = strtol(optarg, NULL, 16);
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            break;
        }
    }
}

int main(int argc, char *argv[]) 
{
    struct usb_dev_handle *hdev;
    unsigned int len = 0;
    unsigned char *write_buf;
    unsigned int remain;
    unsigned int towrite;

    parse_opts(argc, argv);

    if ((hdev = open_usb_port()) == NULL) {
        return 1;
    }

    write_buf = prepare_write_buf(bin_file, &len, load_addr);
    if (write_buf == NULL) {
        return 1;
    }

    printf("loadaddr: 0x%08X\n", load_addr);        
    printf("transferring file to device\n");
    remain = len;
    while (remain) {
        towrite = remain > 512 ? 512 : remain;
        if (towrite != usb_bulk_write(hdev, ep_addr_out, write_buf+(len-remain), towrite, 3000)) {
            printf("usb_bulk_write failed\n");
            break;
        }
        remain -= towrite;
        printf("\r%d%%\t%d bytes", (len-remain)*100/len, len-remain);
        fflush(stdout);
    }
    if (remain == 0) {
        printf("\ncompleted\n");
    }

    return 0;
}


