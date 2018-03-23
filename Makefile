.PHONY: all clean

#export CROSS_COMPILE="/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/"
CROSS_COMPILE=/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/
CC=mipsel-openwrt-linux-uclibc-gcc-4.8.3

INSTALL=install
STRIP=mipsel-openwrt-linux-uclibc-strip

LDFLAGS += -L/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/c-ares-1.10.0/.libs  -lcares
LDFLAGS += -L/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/openssl-1.0.2m  -lssl
LDFLAGS += -L/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/openssl-1.0.2m  -lcrypto
LDFLAGS += -L/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/mosquitto-ssl/mosquitto-1.4.10/lib -lmosquitto

LDFLAGS += -lpthread

CFLAGS = -I/home/fanshuming/qz_proj/qz_mtk_openwrt/openwrt/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/mosquitto-ssl/mosquitto-1.4.10/lib -I./include
CFLAGS += -lpthread -lcurl -lmsc -lasound -lm
#CFLAGS += -lpthread -lcurl -lmsc -lasound -lm

all : spi_play

spi_play : src/main.o src/online_play.o src/user_tts.o src/spim.o crc.o ssap_protocol.o
	#echo "${CROSS_COMPILE}${CC} $^ -o $@ ${LDFLAGS}"
	${CROSS_COMPILE}${CC} $^ -o $@ ${LDFLAGS}


main.o: src/main.c include/user_tts.h
	${CROSS_COMPILE}${CC} ${CFLAGS} -c $< -o $@

spim.o: src/spim.c include/spim.h
	${CROSS_COMPILE}${CC} ${CFLAGS} -c $< -o $@

online_play.o : src/online_play.c include/online_play.h include/crc.h
	${CROSS_COMPILE}${CC} ${CFLAGS} -c $< -o $@ 

crc.o : src/crc.c  include/crc.h
	${CROSS_COMPILE}${CC} ${CFLAGS} -c $< -o $@ 

ssap_protocol.o : src/ssap_protocol.c  include/ssap_protocol.h
	${CROSS_COMPILE}${CC} ${CFLAGS} -c $< -o $@ 

install : all
	$(INSTALL) -d ./bin
	echo "$(INSTALL) -s --strip-program=${CROSS_COMPILE}${STRIP} spi_play bin/spi_play"
	$(INSTALL) -s --strip-program=${CROSS_COMPILE}${STRIP} spi_play bin/spi_play

uninstall :
	-rm -f bin/spi_play

reallyclean : clean

clean : 
	-rm -f src/*.o spi_play

