#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#define IOC_AT24C02_READ 100
#define IOC_AT24C02_WRITE 101
#define IOC_AT24C02_NREAD 102
#define IOC_AT24C02_NWRITE 103


/*
 * at24c02_test /dev/myat24c02 r 10
 * at24c02_test /dev/myat24c02 w 10 123
 */


int main(int argc,char **argv)
{
    int fd;
	int i;
	int buf[10];
	int num[2];
	

	if (argc < 4)
	{
		printf("Usage: %s <dev> r n <addr>\n", argv[0]);
		printf("       %s <dev> w n <addr> <val>\n", argv[0]);
		return -1;
	}


    fd = open(argv[1],O_RDWR);
    if(fd < 0)
    {
        printf("open is err\n");
        return -1;
    }
	

	/* buf[0]大小,buf[1]地址 */
	/* 读一个数值 */
	if(argv[2][0] == 'r')
    {
        buf[0] = strtoul(argv[3], NULL, 0);
		
        ioctl(fd,IOC_AT24C02_READ,buf);
		
		printf("Read addr 0x%x\n", buf[0]);
		printf("get  data %d\n", buf[1]);
		
    }
	/* 读N个数值 */
	else if(argv[2][0] == 'g')
    {
        buf[0] = strtoul(argv[3], NULL, 0);
        buf[1] = strtoul(argv[4], NULL, 0);
		num[0] = buf[0];
		num[1] = buf[1];

		
        ioctl(fd,IOC_AT24C02_NREAD,buf);

		
		printf("Read addr 0x%x\n", num[1]);

		printf("Read nub 0x%x\n", num[0]);
		
		for(i = 0;i < num[0];i++)
			printf("get data 0x%x\n",buf[i]);
		
    }
	/* 写1个数值 */    
	else if(argv[2][0] == 'w')
	{
		buf[0] = strtoul(argv[3], NULL, 0);
		buf[1] = strtoul(argv[4], NULL, 0);
				
		ioctl(fd, IOC_AT24C02_WRITE, buf);
	}	
	/* 写N个数值 */
	else if(argv[2][0] == 'h')
	{
		buf[0] = strtoul(argv[3], NULL, 0);
		buf[1] = strtoul(argv[4], NULL, 0);
		
		for(i = 0;i < buf[0];i++)
			buf[i+2] = strtoul(argv[i+5], NULL, 0);
				
		ioctl(fd, IOC_AT24C02_NWRITE, buf);
	}
    return 0;

}



