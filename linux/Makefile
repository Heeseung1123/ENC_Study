# 1. 빌드 대상 오브젝트 파일 이름 (예: my_driver.c -> my_driver.o)
obj-m += hello.o

# 2. 커널 소스 경로 (아까 준비한 호스트 PC의 소스 경로로 수정하세요)
KDIR := /root/bbb_linux

# 3. 현재 작업 디렉토리 경로
PWD := $(shell pwd)

# 4. 크로스 컴파일러 설정
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

